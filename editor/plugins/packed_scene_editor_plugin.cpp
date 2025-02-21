/**************************************************************************/
/*  packed_scene_editor_plugin.cpp                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "packed_scene_editor_plugin.h"

#include "editor/editor_node.h"
#include "scene/gui/button.h"
#include "scene/resources/packed_scene.h"

void UserInterfaceEditor::_on_open_scene_pressed() {
	// Using deferred call because changing scene updates the Inspector and thus destroys this plugin.
	callable_mp(EditorNode::get_singleton(), &EditorNode::open_request).call_deferred(packed_scene->get_path());
}

void UserInterfaceEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			open_scene_button->set_icon(get_editor_theme_icon(SNAME("UserInterface")));
		} break;
	}
}

UserInterfaceEditor::UserInterfaceEditor(Ref<UserInterface> &p_packed_scene) {
	packed_scene = p_packed_scene;

	open_scene_button = EditorInspector::create_inspector_action_button(TTR("Open Scene"));
	open_scene_button->connect(SceneStringName(pressed), callable_mp(this, &UserInterfaceEditor::_on_open_scene_pressed));
	open_scene_button->set_disabled(!packed_scene->get_path().get_file().is_valid_filename());
	add_child(open_scene_button);

	add_child(memnew(Control)); // Add padding before the regular properties.
}

///////////////////////

bool EditorInspectorPluginUserInterface::can_handle(Object *p_object) {
	return Object::cast_to<UserInterface>(p_object) != nullptr;
}

void EditorInspectorPluginUserInterface::parse_begin(Object *p_object) {
	Ref<UserInterface> packed_scene(p_object);
	UserInterfaceEditor *editor = memnew(UserInterfaceEditor(packed_scene));
	add_custom_control(editor);
}

///////////////////////

UserInterfaceEditorPlugin::UserInterfaceEditorPlugin() {
	Ref<EditorInspectorPluginUserInterface> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}
