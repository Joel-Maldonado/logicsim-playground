#ifndef EDITOR_INPUT_INTERNAL_H
#define EDITOR_INPUT_INTERNAL_H

#include "editor_input.h"

void editor_input_process_solver_frame(
    AppContext *app,
    EditorInputState *state,
    WorkspaceLayoutPrefs *layout_prefs,
    WorkspaceFrame *frame,
    WorkspaceResizeHandles *resize_handles,
    const TopbarLayout *topbar_layout
);

#endif // EDITOR_INPUT_INTERNAL_H
