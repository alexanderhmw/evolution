/*
 * e-mail-display-popup-extension.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#include "e-mail-display-popup-extension.h"
#include "e-mail-display.h"

G_DEFINE_INTERFACE (
	EMailDisplayPopupExtension,
	e_mail_display_popup_extension,
	G_TYPE_OBJECT)

static void
e_mail_display_popup_extension_default_init (EMailDisplayPopupExtensionInterface *iface)
{

}

/**
 * e_mail_display_popup_extension_update_actions:
 *
 * @extension: An object derived from #EMailDisplayPopupExtension
 * @context: A #WebKitHitTestResult describing context of the popup menu
 *
 * When #EMailDisplay is about to display a popup menu, it calls this function
 * on every extension so that they can add their items to the menu.
 */
void
e_mail_display_popup_extension_update_actions (EMailDisplayPopupExtension *extension,
                                               WebKitHitTestResult *context)
{
	EMailDisplayPopupExtensionInterface *iface;

	g_return_if_fail (E_IS_MAIL_DISPLAY_POPUP_EXTENSION (extension));

	iface = E_MAIL_DISPLAY_POPUP_EXTENSION_GET_INTERFACE (extension);
	g_return_if_fail (iface->update_actions != NULL);

	iface->update_actions (extension, context);
}
