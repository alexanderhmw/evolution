# -*- coding: UTF-8 -*-
from behave import step, then
from common_steps import wait_until
from dogtail.tree import root
from dogtail.rawinput import keyCombo
from time import sleep
from os import system


@step(u'Help section "{name}" is displayed')
def help_is_displayed(context, name):
    try:
        context.yelp = root.application('yelp')
        frame = context.yelp.child(roleName='frame')
        wait_until(lambda x: x.showing, frame)
        sleep(1)
        context.assertion.assertEquals(name, frame.name)
    finally:
        system("killall yelp")


@step(u'Evolution has {num:d} window opened')
@step(u'Evolution has {num:d} windows opened')
def evolution_has_num_windows_opened(context, num):
    windows = context.app.findChildren(lambda x: x.roleName == 'frame')
    context.assertion.assertEqual(len(windows), num)


@step(u'Preferences dialog is opened')
def preferences_dialog_opened(context):
    context.app.window('Evolution Preferences')


@step(u'"{name}" view is opened')
def view_is_opened(context, name):
    if name != 'Mail':
        window_name = context.app.children[0].name
        context.assertion.assertEquals(window_name, "%s - Evolution" % name)
    else:
        # A special case for Mail
        context.assertion.assertTrue(context.app.menu('Message').showing)


@step(u'Open "{section_name}" section')
def open_section_by_name(context, section_name):
    context.app.menu('View').click()
    context.app.menu('View').menu('Window').point()
    context.app.menu('View').menu('Window').menuItem(section_name).click()


@step(u'"{name}" menu is opened')
def menu_is_opened(context, name):
    sleep(0.5)
    menu = context.app.menu(name)
    children_displayed = [x.showing for x in menu.children]
    context.assertion.assertTrue(True in children_displayed, "Menu '%s' is not opened" % name)


@step(u'Press "{sequence}"')
def press_button_sequence(context, sequence):
    keyCombo(sequence)
    sleep(0.5)


@then(u'Evolution is closed')
def evolution_is_closed(context):
    assert wait_until(lambda x: x.dead, context.app),\
        "Evolution window is opened"
    context.assertion.assertFalse(context.app_class.isRunning(), "Evolution is in the process list")


@step(u'Message composer with title "{name}" is opened')
def message_composer_is_opened(context, name):
    context.app.composer = context.app.window(name)


@then(u'Contact editor window with title "{title}" is opened')
def contact_editor_with_label_is_opened(context, title):
    context.app.contact_editor = context.app.dialog(title)
    context.assertion.assertIsNotNone(
        context.app.contact_editor, "Contact Editor was not found")
    context.assertion.assertTrue(
        context.app.contact_editor.showing, "Contact Editor didn't appear")


@then(u'Contact editor window is opened')
def contact_editor_is_opened(context):
    context.execute_steps(u'Then Contact editor window with title "Contact Editor" is opened')


@then(u'Contact List editor window is opened')
def contact_list_editor_is_opened(context):
    context.execute_steps(
        u'Then Contact List editor window with title "Contact List Editor" is opened')


@then(u'Contact List editor window with title "{name}" is opened')
def contact_list_editor__with_name_is_opened(context, name):
    context.app.contact_list_editor = context.app.dialog(name)


@step(u'Memo editor with title "{name}" is opened')
def memo_editor_is_opened(context, name):
    context.execute_steps(u'* Task editor with title "%s" is opened' % name)


@step(u'Shared Memo editor with title "{name}" is opened')
def shared_memo_editor_is_opened(context, name):
    context.execute_steps(u'* Task editor with title "%s" is opened' % name)


@step(u'Task editor with title "{title}" is opened')
def task_editor_with_title_is_opened(context, title):
    context.app.task_editor = context.app.window(title)
    # Spoof event_editor for assigned tasks
    if 'Assigned' in title:
        context.app.event_editor = context.app.task_editor


@step(u'Event editor with title "{name}" is displayed')
def event_editor_with_name_displayed(context, name):
    context.app.event_editor = context.app.window(name)