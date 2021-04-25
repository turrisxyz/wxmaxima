// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode: nil -*-
//
//  Copyright (C) 2004-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
//            (C) 2016-2018 Gunter Königsmann <wxMaxima@physikbuch.de>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
//
//  SPDX-License-Identifier: GPL-2.0+

/*! \file
  This file defines the class MathPrintOut

  MathPrintOut is the class that handles printing.
*/

//! Bitmaps are scaled down if the resolution of the DC is too low.
#define DPI_REFERENCE 96.0

#include "Printout.h"

#include <wx/config.h>
#include <wx/busyinfo.h>
#include <wx/log.h>

#define PRINT_MARGIN_HORIZONTAL 50
#define PRINT_MARGIN_VERTICAL 50

Printout::Printout(wxString title, Configuration **configuration, double scaleFactor) : wxPrintout(title)
{
  m_scaleFactor = scaleFactor;
  m_configuration = configuration;
  m_oldconfig = *m_configuration;
  m_numberOfPages = 0;
  m_printConfigCreated = false;
}

Printout::~Printout()
{
  DestroyTree();
  if(m_printConfigCreated)
    wxDELETE(*m_configuration);
  *m_configuration = m_oldconfig;
  (*m_configuration)->FontChanged();
}

void Printout::SetData(std::unique_ptr<GroupCell> &&tree)
{
  m_tree = std::move(tree);
}

bool Printout::HasPage(int num)
{
  if (num > 0 && num <= m_numberOfPages)
    return true;
  return false;
}

bool Printout::OnPrintPage(int num)
{
//  wxBusyInfo busyInfo(wxString::Format(_("Printing page %i..."),num));
  wxDC *dc = GetDC();
  dc->SetBackground(*wxWHITE_BRUSH);
  dc->Clear();
  
  int pageWidth, pageHeight;
  int marginX, marginY;
  GetPageSizePixels(&pageWidth, &pageHeight);
  GetPageMargins(&marginX, &marginY);
  (*m_configuration)->SetCanvasSize({pageWidth - marginX, pageHeight - marginY});
  
  // Make sure that during print nothing is outside the crop rectangle

  marginX += (*m_configuration)->Scale_Px((*m_configuration)->GetBaseIndent());

  GroupCell *tmp = m_pages[num - 1]->GetGroup();
  if (!tmp)
    return false;
  if (tmp->GetGroupType() == GC_TYPE_PAGEBREAK)
    tmp = tmp->GetNext();
  if (!tmp)
    return true;

  // Print the header
  dc->SetDeviceOrigin(0,0);
  wxPoint point;
  point.x = marginX;
  point.y = marginY + tmp->GetCenterList() + GetHeaderHeight();
  wxConfigBase *config = wxConfig::Get();
  int fontsize = 12;
  int drop = tmp->GetMaxDrop();

  config->Read(wxT("fontsize"), &fontsize);

  PrintHeader(num, dc);

  // Print the page contents
  dc->SetDeviceOrigin(
    marginX,
    marginY + GetHeaderHeight() - m_pages[num - 1]->GetCurrentPoint().y +
    (*m_configuration)->Scale_Px((*m_configuration)->GetGroupSkip())
    );

  Cell *end;
  if(m_pages.size() > num)
  {
    end = m_pages[num];
    dc->DestroyClippingRegion();
    int startpoint = m_pages[num - 1]->GetCurrentPoint().y - m_pages[num - 1]->GetMaxDrop();
    dc->SetClippingRegion(0, startpoint,
                          pageWidth,
                          end->GetCurrentPoint().y - startpoint +end->GetCenterList());
  }
  else
  {
    dc->SetClippingRegion(0,
                          m_pages[num - 1]->GetCurrentPoint().y - m_pages[num - 1]->GetMaxDrop(),
                          pageWidth,
                          pageHeight);
    end = NULL;
  }
  
  while (tmp &&
         (tmp->GetGroupType() != GC_TYPE_PAGEBREAK))
  {
    auto *const next = tmp->GetNext();
    point = tmp->GetCurrentPoint();

    // The following line seems to mysteriously fix the "subsequent text
    // cells aren't printed" problem on linux.
    // No Idea why, though.
    dc->SetPen(wxPen(*wxBLACK, 1, wxPENSTYLE_SOLID));

    tmp->Draw(point);

    if(end && (tmp == end->GetGroup()))
      break;
    tmp = tmp->GetNext();
  }
  return true;
}

bool Printout::OnBeginDocument(int startPage, int endPage)
{
  if (!wxPrintout::OnBeginDocument(startPage, endPage))
    return false;
  return true;
}

void Printout::BreakPages()
{
  if (m_tree == NULL)
    return;

  int pageWidth, pageHeight;
  int marginX, marginY;
  int headerHeight = GetHeaderHeight();

  GetPageMargins(&marginX, &marginY);
  GetPageSizePixels(&pageWidth, &pageHeight);

  wxCoord pageStart;
  wxCoord maxContentHeight = pageHeight - 2 * marginY;
  int skip = (*m_configuration)->Scale_Px((*m_configuration)->GetGroupSkip());;

  GroupCell *group = m_tree.get();
  m_pages.push_back(group);
  if(group)
    pageStart =group->GetCurrentPoint().y;
  m_numberOfPages = 1;
  for (GroupCell &group : OnList(m_tree.get()))
  {
    if(group.GetGroupType() == GC_TYPE_PAGEBREAK)
    {
      m_pages.push_back(&group);
      m_numberOfPages++;
      pageStart =group.GetCurrentPoint().y;
      continue;
    }
    if (group.GetCurrentPoint().y - pageStart >= maxContentHeight)
    {
      if(group.GetCurrentPoint().y - pageStart >= .7*maxContentHeight)
      {
        m_pages.push_back(&group);
        wxLogMessage(wxString::Format(
                       _("printout: Page %i filled to > 70\% ⇒ Flushing the page."),
                       m_numberOfPages));
        m_numberOfPages++;
        pageStart =group.GetCurrentPoint().y;
        continue;
      }
      if (group.GetPrompt()->GetCurrentPoint().y+group.GetPrompt()->GetHeightList() - pageStart >=
          maxContentHeight)
      {
        wxLogMessage(_("printout: The next input cell doesn't fit on the current page"));
        m_pages.push_back(&group);
        m_numberOfPages++;
        pageStart = group.GetCurrentPoint().y;
        continue;
      }
      else
      {
        wxLogMessage(_("printout: Input of next cell still fits on page!"));        
        Cell *out = group.GetOutput();
        Cell *pageEnd = out;
        while(out &&
              (out->GetCurrentPoint().y-pageStart < maxContentHeight))
        {
          wxLogMessage(_("printout: Found a output line that still fits on page!"));
          pageEnd = out;
          while(out && (!out->BreakLineHere()))
            out = out->GetNext();
        }
        m_pages.push_back(pageEnd);
        m_numberOfPages++;
      }
    }
  }
}

void Printout::SetupData()
{
  wxDC *dc = GetDC();
  *m_configuration = new Configuration(dc, Configuration::temporary);
  // Make sure that during print nothing is outside the crop rectangle
  (*m_configuration)->LineWidth_em(10000);
  
  m_printConfigCreated = true;
  (*m_configuration)->ShowCodeCells(m_oldconfig->ShowCodeCells());
  (*m_configuration)->ShowBrackets((*m_configuration)->PrintBrackets());
  (*m_configuration)->ClipToDrawRegion(false);
  
//  SetUserScale(1/DCSCALE,
//               1/DCSCALE);
  // on MSW according to https://groups.google.com/forum/#!topic/wx-users/QF_W4g3Oe98
  // the wxFont::SetPointSize is scaled relative to the screen DPI rate in order to
  // get the right font size in pixels. Unfortunately this is true for printing, too,
  // which might employ an entirely different DPI rate.
  //
  // Also it could be shown that on a 600dpi printer the font is only half the size
  // one would get on an 300dpi printer => we need to correct the scale factor for
  // the DPI rate, too. It seems that for a 75dpi and a 300dpi printer the scaling
  // factor is 1.0.
  wxSize printPPI;
  printPPI = (*m_configuration)->GetDC()->GetPPI();
  if(printPPI.x < 1)
    printPPI.x = 72;
  if(printPPI.y < 1)
    printPPI.y = 72;
  (*m_configuration)->GetDC()->SetUserScale(1.0,1.0);
  (*m_configuration)->SetZoomFactor_temporarily(
    printPPI.x / DPI_REFERENCE * m_oldconfig->PrintScale() / m_scaleFactor
  );

  // wxSize screenPPI;
  // screenPPI = m_oldconfig->GetDC()->GetPPI();
  // double oldZoomFactor = m_oldconfig->GetZoomFactor();
  // wxMessageDialog dialog(NULL,
  //   wxString::Format(wxT("screenPPI.x=%i,\nprintPPI.x=%i\nzoomFactor=%f\nUserScale.x=%f"),
  //     screenPPI.x, printPPI.x, oldZoomFactor, userScale_x),
  //   wxString("Printer Parameters"));
  // dialog.ShowModal();

  int pageWidth, pageHeight;
  int marginX, marginY;
  GetPageSizePixels(&pageWidth, &pageHeight);
  GetPageMargins(&marginX, &marginY);

  (*m_configuration)->SetClientWidth(pageWidth - 2 * marginX
    - (*m_configuration)->Scale_Px(72) // Some additional margin to compensate for title and section indent
    - (*m_configuration)->Scale_Px((*m_configuration)->GetBaseIndent()));
  (*m_configuration)->SetClientHeight(pageHeight - 2 * marginY);

  if((*m_configuration)->PrintBrackets())
  {
    if(marginX < (*m_configuration)->Scale_Px(1 + (*m_configuration)->GetBaseIndent()))
      marginX = (*m_configuration)->Scale_Px(1 + (*m_configuration)->GetBaseIndent());
  }
  (*m_configuration)->SetIndent(marginX);
  // Inform the output routines that we are printing
  (*m_configuration)->SetPrinting(true);
  // Make sure that during print nothing is outside the crop rectangle
  (*m_configuration)->LineWidth_em(10000);
  Recalculate();
  BreakPages();
}

void Printout::GetPageInfo(int *minPage, int *maxPage,
                               int *fromPage, int *toPage)
{
  *minPage = 1;
  *maxPage = m_numberOfPages;
  *fromPage = 1;
  *toPage = m_numberOfPages;
}

void Printout::OnPreparePrinting()
{
  SetupData();
}

void Printout::GetPageMargins(int *horizontal, int *vertical)
{
  *horizontal = (int) ((*m_configuration)->Scale_Px(PRINT_MARGIN_HORIZONTAL));
  *vertical = (int) ((*m_configuration)->Scale_Px(PRINT_MARGIN_VERTICAL));
}

int Printout::GetHeaderHeight()
{
  wxDC *dc = GetDC();
  int width, height;

  dc->SetFont(wxFont((*m_configuration)->Scale_Px(10), wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
  dc->GetTextExtent(GetTitle(), &width, &height);
  return height + (*m_configuration)->Scale_Px(12);
}

void Printout::PrintHeader(int pageNum, wxDC *dc)
{
  int page_width, page_height;
  int title_width, title_height;
  int marginX, marginY;
  int pageWidth, pageHeight;

  GetPageMargins(&marginX, &marginY);
  GetPageSizePixels(&pageWidth, &pageHeight);

  dc->SetTextForeground(wxColour(wxT("grey")));
  dc->SetPen(wxPen(wxT("light grey"), (*m_configuration)->Scale_Px(1), wxPENSTYLE_SOLID));

  dc->SetFont(wxFont((*m_configuration)->Scale_Px(10), wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
  dc->GetTextExtent(GetTitle(), &title_width, &title_height);
  wxString page = wxString::Format(wxT("%d / %d"), pageNum, m_numberOfPages);
  dc->GetTextExtent(page, &page_width, &page_height);

  dc->DrawText(GetTitle(), marginX, marginY);
  dc->DrawText(page, pageWidth - page_width - marginX, marginY);

  dc->DrawLine(marginX, marginY + title_height + (*m_configuration)->Scale_Px(3),
               pageWidth - marginX, marginY + title_height + (*m_configuration)->Scale_Px(3));

  dc->SetTextForeground(wxColour(wxT("black")));
  dc->SetPen(wxPen(wxT("black"), 1, wxPENSTYLE_SOLID));
}

void Printout::Recalculate()
{
  if(!m_tree)
    return;
  
  int marginX, marginY;
  GetPageMargins(&marginX, &marginY);
  int pageWidth, pageHeight;
  GetPageSizePixels(&pageWidth, &pageHeight);

//  marginX += (*m_configuration)->Scale_Px((*m_configuration)->GetBaseIndent());

  m_tree -> ResetDataList();

  for (GroupCell &group : OnList(m_tree.get()))
    group.Recalculate();

  m_tree->UpdateYPositionList();
}

void Printout::DestroyTree()
{
  m_tree.reset();
}
