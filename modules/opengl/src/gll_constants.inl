/* gll_constants.inl — OpenGL 1.x / 2.x Legacy constants
** Included by zen_gl_legacy.cpp */

static const NativeConst gll_constants[] = {
    /* Primitive types (também em gl, mas conveniente ter aqui) */
    {"GL_POINTS",         val_int(GL_POINTS)},
    {"GL_LINES",          val_int(GL_LINES)},
    {"GL_LINE_LOOP",      val_int(GL_LINE_LOOP)},
    {"GL_LINE_STRIP",     val_int(GL_LINE_STRIP)},
    {"GL_TRIANGLES",      val_int(GL_TRIANGLES)},
    {"GL_TRIANGLE_STRIP", val_int(GL_TRIANGLE_STRIP)},
    {"GL_TRIANGLE_FAN",   val_int(GL_TRIANGLE_FAN)},
    {"GL_QUADS",          val_int(GL_QUADS)},
    {"GL_QUAD_STRIP",     val_int(GL_QUAD_STRIP)},
    {"GL_POLYGON",        val_int(GL_POLYGON)},

    /* Matrix modes */
    {"GL_MODELVIEW",  val_int(GL_MODELVIEW)},
    {"GL_PROJECTION", val_int(GL_PROJECTION)},
    {"GL_TEXTURE",    val_int(GL_TEXTURE)},
    {"GL_COLOR",      val_int(GL_COLOR)},

    /* Matrix getters */
    {"GL_MODELVIEW_MATRIX",  val_int(GL_MODELVIEW_MATRIX)},
    {"GL_PROJECTION_MATRIX", val_int(GL_PROJECTION_MATRIX)},
    {"GL_TEXTURE_MATRIX",    val_int(GL_TEXTURE_MATRIX)},

    /* Shade model */
    {"GL_FLAT",   val_int(GL_FLAT)},
    {"GL_SMOOTH", val_int(GL_SMOOTH)},

    /* Lighting caps */
    {"GL_LIGHTING",         val_int(GL_LIGHTING)},
    {"GL_LIGHT0",           val_int(GL_LIGHT0)},
    {"GL_LIGHT1",           val_int(GL_LIGHT1)},
    {"GL_LIGHT2",           val_int(GL_LIGHT2)},
    {"GL_LIGHT3",           val_int(GL_LIGHT3)},
    {"GL_LIGHT4",           val_int(GL_LIGHT4)},
    {"GL_LIGHT5",           val_int(GL_LIGHT5)},
    {"GL_LIGHT6",           val_int(GL_LIGHT6)},
    {"GL_LIGHT7",           val_int(GL_LIGHT7)},
    {"GL_COLOR_MATERIAL",   val_int(GL_COLOR_MATERIAL)},
    {"GL_NORMALIZE",        val_int(GL_NORMALIZE)},
    {"GL_RESCALE_NORMAL",   val_int(GL_RESCALE_NORMAL)},

    /* Light parameters */
    {"GL_AMBIENT",               val_int(GL_AMBIENT)},
    {"GL_DIFFUSE",               val_int(GL_DIFFUSE)},
    {"GL_SPECULAR",              val_int(GL_SPECULAR)},
    {"GL_POSITION",              val_int(GL_POSITION)},
    {"GL_SPOT_DIRECTION",        val_int(GL_SPOT_DIRECTION)},
    {"GL_SPOT_EXPONENT",         val_int(GL_SPOT_EXPONENT)},
    {"GL_SPOT_CUTOFF",           val_int(GL_SPOT_CUTOFF)},
    {"GL_CONSTANT_ATTENUATION",  val_int(GL_CONSTANT_ATTENUATION)},
    {"GL_LINEAR_ATTENUATION",    val_int(GL_LINEAR_ATTENUATION)},
    {"GL_QUADRATIC_ATTENUATION", val_int(GL_QUADRATIC_ATTENUATION)},

    /* Material parameters */
    {"GL_EMISSION",  val_int(GL_EMISSION)},
    {"GL_SHININESS", val_int(GL_SHININESS)},
    {"GL_AMBIENT_AND_DIFFUSE", val_int(GL_AMBIENT_AND_DIFFUSE)},
    {"GL_COLOR_INDEXES",       val_int(GL_COLOR_INDEXES)},

    /* LightModel */
    {"GL_LIGHT_MODEL_AMBIENT",       val_int(GL_LIGHT_MODEL_AMBIENT)},
    {"GL_LIGHT_MODEL_LOCAL_VIEWER",  val_int(GL_LIGHT_MODEL_LOCAL_VIEWER)},
    {"GL_LIGHT_MODEL_TWO_SIDE",      val_int(GL_LIGHT_MODEL_TWO_SIDE)},
    {"GL_LIGHT_MODEL_COLOR_CONTROL", val_int(GL_LIGHT_MODEL_COLOR_CONTROL)},
    {"GL_SINGLE_COLOR",              val_int(GL_SINGLE_COLOR)},
    {"GL_SEPARATE_SPECULAR_COLOR",   val_int(GL_SEPARATE_SPECULAR_COLOR)},

    /* Fog */
    {"GL_FOG",         val_int(GL_FOG)},
    {"GL_FOG_MODE",    val_int(GL_FOG_MODE)},
    {"GL_FOG_COLOR",   val_int(GL_FOG_COLOR)},
    {"GL_FOG_DENSITY", val_int(GL_FOG_DENSITY)},
    {"GL_FOG_START",   val_int(GL_FOG_START)},
    {"GL_FOG_END",     val_int(GL_FOG_END)},
    {"GL_FOG_INDEX",   val_int(GL_FOG_INDEX)},
    {"GL_LINEAR",      val_int(GL_LINEAR)},
    {"GL_EXP",         val_int(GL_EXP)},
    {"GL_EXP2",        val_int(GL_EXP2)},

    /* Alpha test */
    {"GL_ALPHA_TEST",      val_int(GL_ALPHA_TEST)},
    {"GL_ALPHA_TEST_FUNC", val_int(GL_ALPHA_TEST_FUNC)},
    {"GL_ALPHA_TEST_REF",  val_int(GL_ALPHA_TEST_REF)},

    /* Stipple */
    {"GL_LINE_STIPPLE",    val_int(GL_LINE_STIPPLE)},
    {"GL_POLYGON_STIPPLE", val_int(GL_POLYGON_STIPPLE)},

    /* Display lists */
    {"GL_COMPILE",             val_int(GL_COMPILE)},
    {"GL_COMPILE_AND_EXECUTE", val_int(GL_COMPILE_AND_EXECUTE)},
    {"GL_LIST_BASE",           val_int(GL_LIST_BASE)},
    {"GL_LIST_INDEX",          val_int(GL_LIST_INDEX)},
    {"GL_LIST_MODE",           val_int(GL_LIST_MODE)},

    /* Push/Pop attrib bits */
    {"GL_CURRENT_BIT",         val_int(GL_CURRENT_BIT)},
    {"GL_POINT_BIT",           val_int(GL_POINT_BIT)},
    {"GL_LINE_BIT",            val_int(GL_LINE_BIT)},
    {"GL_POLYGON_BIT",         val_int(GL_POLYGON_BIT)},
    {"GL_POLYGON_STIPPLE_BIT", val_int(GL_POLYGON_STIPPLE_BIT)},
    {"GL_PIXEL_MODE_BIT",      val_int(GL_PIXEL_MODE_BIT)},
    {"GL_LIGHTING_BIT",        val_int(GL_LIGHTING_BIT)},
    {"GL_FOG_BIT",             val_int(GL_FOG_BIT)},
    {"GL_DEPTH_BUFFER_BIT",    val_int(GL_DEPTH_BUFFER_BIT)},
    {"GL_ACCUM_BUFFER_BIT",    val_int(GL_ACCUM_BUFFER_BIT)},
    {"GL_STENCIL_BUFFER_BIT",  val_int(GL_STENCIL_BUFFER_BIT)},
    {"GL_VIEWPORT_BIT",        val_int(GL_VIEWPORT_BIT)},
    {"GL_TRANSFORM_BIT",       val_int(GL_TRANSFORM_BIT)},
    {"GL_ENABLE_BIT",          val_int(GL_ENABLE_BIT)},
    {"GL_COLOR_BUFFER_BIT",    val_int(GL_COLOR_BUFFER_BIT)},
    {"GL_HINT_BIT",            val_int(GL_HINT_BIT)},
    {"GL_EVAL_BIT",            val_int(GL_EVAL_BIT)},
    {"GL_LIST_BIT",            val_int(GL_LIST_BIT)},
    {"GL_TEXTURE_BIT",         val_int(GL_TEXTURE_BIT)},
    {"GL_SCISSOR_BIT",         val_int(GL_SCISSOR_BIT)},
    {"GL_ALL_ATTRIB_BITS",     val_int(GL_ALL_ATTRIB_BITS)},

    /* Render mode */
    {"GL_RENDER",   val_int(GL_RENDER)},
    {"GL_SELECT",   val_int(GL_SELECT)},
    {"GL_FEEDBACK", val_int(GL_FEEDBACK)},

    /* Accumulation ops */
    {"GL_ACCUM",  val_int(GL_ACCUM)},
    {"GL_LOAD",   val_int(GL_LOAD)},
    {"GL_RETURN", val_int(GL_RETURN)},
    {"GL_MULT",   val_int(GL_MULT)},
    {"GL_ADD",    val_int(GL_ADD)},

    /* TexGen */
    {"GL_TEXTURE_GEN_S",   val_int(GL_TEXTURE_GEN_S)},
    {"GL_TEXTURE_GEN_T",   val_int(GL_TEXTURE_GEN_T)},
    {"GL_TEXTURE_GEN_R",   val_int(GL_TEXTURE_GEN_R)},
    {"GL_TEXTURE_GEN_Q",   val_int(GL_TEXTURE_GEN_Q)},
    {"GL_TEXTURE_GEN_MODE",val_int(GL_TEXTURE_GEN_MODE)},
    {"GL_EYE_PLANE",       val_int(GL_EYE_PLANE)},
    {"GL_OBJECT_PLANE",    val_int(GL_OBJECT_PLANE)},
    {"GL_SPHERE_MAP",      val_int(GL_SPHERE_MAP)},
    {"GL_REFLECTION_MAP",  val_int(GL_REFLECTION_MAP)},
    {"GL_NORMAL_MAP",      val_int(GL_NORMAL_MAP)},

    /* ColorMaterial face/mode */
    {"GL_FRONT",          val_int(GL_FRONT)},
    {"GL_BACK",           val_int(GL_BACK)},
    {"GL_FRONT_AND_BACK", val_int(GL_FRONT_AND_BACK)},

    /* Client state (glEnableClientState / glDisableClientState) */
    {"GL_VERTEX_ARRAY",        val_int(GL_VERTEX_ARRAY)},
    {"GL_NORMAL_ARRAY",        val_int(GL_NORMAL_ARRAY)},
    {"GL_COLOR_ARRAY",         val_int(GL_COLOR_ARRAY)},
    {"GL_TEXTURE_COORD_ARRAY", val_int(GL_TEXTURE_COORD_ARRAY)},
    {"GL_INDEX_ARRAY",         val_int(GL_INDEX_ARRAY)},
    {"GL_EDGE_FLAG_ARRAY",     val_int(GL_EDGE_FLAG_ARRAY)},

    /* Data types usados em glVertexPointer etc. */
    {"GL_BYTE",           val_int(GL_BYTE)},
    {"GL_UNSIGNED_BYTE",  val_int(GL_UNSIGNED_BYTE)},
    {"GL_SHORT",          val_int(GL_SHORT)},
    {"GL_UNSIGNED_SHORT", val_int(GL_UNSIGNED_SHORT)},
    {"GL_INT",            val_int(GL_INT)},
    {"GL_UNSIGNED_INT",   val_int(GL_UNSIGNED_INT)},
    {"GL_FLOAT",          val_int(GL_FLOAT)},
    {"GL_DOUBLE",         val_int(GL_DOUBLE)},

    /* Interleaved array formats (glInterleavedArrays) */
    {"GL_V2F",          val_int(GL_V2F)},
    {"GL_V3F",          val_int(GL_V3F)},
    {"GL_C4UB_V2F",     val_int(GL_C4UB_V2F)},
    {"GL_C4UB_V3F",     val_int(GL_C4UB_V3F)},
    {"GL_C3F_V3F",      val_int(GL_C3F_V3F)},
    {"GL_N3F_V3F",      val_int(GL_N3F_V3F)},
    {"GL_C4F_N3F_V3F",  val_int(GL_C4F_N3F_V3F)},
    {"GL_T2F_V3F",      val_int(GL_T2F_V3F)},
    {"GL_T4F_V4F",      val_int(GL_T4F_V4F)},
    {"GL_T2F_C4UB_V3F", val_int(GL_T2F_C4UB_V3F)},
    {"GL_T2F_C3F_V3F",  val_int(GL_T2F_C3F_V3F)},
    {"GL_T2F_N3F_V3F",  val_int(GL_T2F_N3F_V3F)},
    {"GL_T2F_C4F_N3F_V3F", val_int(GL_T2F_C4F_N3F_V3F)},
    {"GL_T4F_C4F_N3F_V4F", val_int(GL_T4F_C4F_N3F_V4F)},
};
