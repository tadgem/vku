#pragma once

// User-defined assertion handler (default is cassert assert()).
//#define IM3D_ASSERT(e) assert(e)

// User-defined malloc/free. Define both or neither (default is cstdlib malloc()/free()).
//#define IM3D_MALLOC(size) malloc(size)
//#define IM3D_FREE(ptr) free(ptr)

// User-defined API declaration (e.g. __declspec(dllexport)).
//#define IM3D_API

// Use a thread-local context pointer.
//#define IM3D_THREAD_LOCAL_CONTEXT_PTR 1

// Use row-major internal matrix layout.
//#define IM3D_MATRIX_ROW_MAJOR 1

// Force vertex data alignment (default is 4 bytes).
#define IM3D_VERTEX_ALIGNMENT 32

// Enable internal culling for primitives (everything drawn between Begin*()/End()). The application must set a culling frustum via AppData.
//#define IM3D_CULL_PRIMITIVES 1

// Enable internal culling for gizmos. The application must set a culling frustum via AppData.
//#define IM3D_CULL_GIZMOS 1

// Conversion to/from application math types.

