#ifndef INCLUDE_RENDER_QUAD_H
#define INCLUDE_RENDER_QUAD_H

struct RenderQuad_;
typedef struct RenderQuad_* RenderQuad;

RenderQuad create_render_quad(void);
void delete_render_quad(RenderQuad primitives_buffers);

#endif
