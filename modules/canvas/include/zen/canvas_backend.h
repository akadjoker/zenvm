#pragma once
/* =========================================================
** zen/canvas_backend.h — C++ host access to canvas DrawList
**
** Include this to read the raw DrawList from a Zen DrawList instance
** (e.g. for a custom non-GL backend, or for debug inspection).
**
** For the built-in GL backend just call canvas.backend_init(w,h)
** from Zen and use dl.flush() — no need for this header.
**
** Link against zen_module_canvas.
** ========================================================= */

#include "canvas_draw.h"   /* canvas::DrawList, canvas::Vertex, canvas::Cmd */
#include "value.h"         /* Value, is_instance, as_instance               */

namespace zen
{

/* canvas_get_drawlist(v)
**
** Given a Zen Value that holds a DrawList instance (created by canvas.new()),
** returns a pointer to the underlying canvas::DrawList.
** Returns nullptr if v is not a canvas DrawList instance. */
inline canvas::DrawList *canvas_get_drawlist(Value v)
{
    if (!is_instance(v))
        return nullptr;
    ObjInstance *inst = as_instance(v);
    if (!inst || !inst->native_data)
        return nullptr;
    /* CanvasCtx is standard-layout with DrawList as first member */
    return reinterpret_cast<canvas::DrawList *>(inst->native_data);
}

} /* namespace zen */
