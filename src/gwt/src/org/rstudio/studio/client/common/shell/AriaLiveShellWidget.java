/*
 * AriaLiveShellWidget.java
 *
 * Copyright (C) 2020 by RStudio, Inc.
 *
 * Unless you have received this program directly from RStudio pursuant
 * to the terms of a commercial license agreement with RStudio, then
 * this program is licensed to you under the terms of version 3 of the
 * GNU Affero General Public License. This program is distributed WITHOUT
 * ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THOSE OF NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Please refer to the
 * AGPL (http://www.gnu.org/licenses/agpl-3.0.txt) for more details.
 *
 */
package org.rstudio.studio.client.common.shell;

import com.google.gwt.aria.client.LiveValue;
import com.google.gwt.aria.client.Roles;
import com.google.gwt.dom.client.Document;
import com.google.gwt.dom.client.Text;
import com.google.gwt.user.client.ui.Widget;
import org.rstudio.core.client.a11y.A11y;

/**
 * Visibly-hidden live region used to report recent console output to a screen reader.
 */
public class AriaLiveShellWidget extends Widget
{
   private static final int LINE_REPORTING_LIMIT = 25;
   public AriaLiveShellWidget()
   {
      lineCount_ = 0;
      setElement(Document.get().createDivElement());
      A11y.setVisuallyHidden(getElement());
      Roles.getStatusRole().setAriaLiveProperty(getElement(), LiveValue.ASSERTIVE);
   }
   
   public void announce(String text)
   {
      // TODO (gary) line counting
      Text textNode = Document.get().createTextNode(text);
      getElement().appendChild(textNode);
      lineCount_++;
   }

   public void clearLiveRegion()
   {
      getElement().setInnerText("");
      lineCount_ = 0;
   }
   
   private int lineCount_;
}
