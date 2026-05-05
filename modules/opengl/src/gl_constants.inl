/* gl_constants.inl — OpenGL constants as NativeConst array (GLES 3.0 compatible)
** Included by zen_gl.cpp */

static const NativeConst gl_constants[] = {
    /* Clear bits */
    {"GL_COLOR_BUFFER_BIT",   val_int(GL_COLOR_BUFFER_BIT)},
    {"GL_DEPTH_BUFFER_BIT",   val_int(GL_DEPTH_BUFFER_BIT)},
    {"GL_STENCIL_BUFFER_BIT", val_int(GL_STENCIL_BUFFER_BIT)},

    /* Primitive types */
    {"GL_POINTS",         val_int(GL_POINTS)},
    {"GL_LINES",          val_int(GL_LINES)},
    {"GL_LINE_LOOP",      val_int(GL_LINE_LOOP)},
    {"GL_LINE_STRIP",     val_int(GL_LINE_STRIP)},
    {"GL_TRIANGLES",      val_int(GL_TRIANGLES)},
    {"GL_TRIANGLE_STRIP", val_int(GL_TRIANGLE_STRIP)},
    {"GL_TRIANGLE_FAN",   val_int(GL_TRIANGLE_FAN)},

    /* Enable/disable caps */
    {"GL_BLEND",        val_int(GL_BLEND)},
    {"GL_CULL_FACE",    val_int(GL_CULL_FACE)},
    {"GL_DEPTH_TEST",   val_int(GL_DEPTH_TEST)},
    {"GL_DITHER",       val_int(GL_DITHER)},
    {"GL_SCISSOR_TEST", val_int(GL_SCISSOR_TEST)},
    {"GL_STENCIL_TEST", val_int(GL_STENCIL_TEST)},
#ifdef GL_POLYGON_OFFSET_FILL
    {"GL_POLYGON_OFFSET_FILL", val_int(GL_POLYGON_OFFSET_FILL)},
#endif
#ifdef GL_SAMPLE_COVERAGE
    {"GL_SAMPLE_COVERAGE",     val_int(GL_SAMPLE_COVERAGE)},
#endif
#ifdef GL_SAMPLE_ALPHA_TO_COVERAGE
    {"GL_SAMPLE_ALPHA_TO_COVERAGE", val_int(GL_SAMPLE_ALPHA_TO_COVERAGE)},
#endif

    /* Blend functions */
    {"GL_ZERO",                     val_int(GL_ZERO)},
    {"GL_ONE",                      val_int(GL_ONE)},
    {"GL_SRC_COLOR",                val_int(GL_SRC_COLOR)},
    {"GL_ONE_MINUS_SRC_COLOR",      val_int(GL_ONE_MINUS_SRC_COLOR)},
    {"GL_DST_COLOR",                val_int(GL_DST_COLOR)},
    {"GL_ONE_MINUS_DST_COLOR",      val_int(GL_ONE_MINUS_DST_COLOR)},
    {"GL_SRC_ALPHA",                val_int(GL_SRC_ALPHA)},
    {"GL_ONE_MINUS_SRC_ALPHA",      val_int(GL_ONE_MINUS_SRC_ALPHA)},
    {"GL_DST_ALPHA",                val_int(GL_DST_ALPHA)},
    {"GL_ONE_MINUS_DST_ALPHA",      val_int(GL_ONE_MINUS_DST_ALPHA)},
    {"GL_CONSTANT_COLOR",           val_int(GL_CONSTANT_COLOR)},
    {"GL_ONE_MINUS_CONSTANT_COLOR", val_int(GL_ONE_MINUS_CONSTANT_COLOR)},
    {"GL_CONSTANT_ALPHA",           val_int(GL_CONSTANT_ALPHA)},
    {"GL_ONE_MINUS_CONSTANT_ALPHA", val_int(GL_ONE_MINUS_CONSTANT_ALPHA)},
    {"GL_SRC_ALPHA_SATURATE",       val_int(GL_SRC_ALPHA_SATURATE)},

    /* Blend equations */
    {"GL_FUNC_ADD",              val_int(GL_FUNC_ADD)},
    {"GL_FUNC_SUBTRACT",         val_int(GL_FUNC_SUBTRACT)},
    {"GL_FUNC_REVERSE_SUBTRACT", val_int(GL_FUNC_REVERSE_SUBTRACT)},
    {"GL_MIN",                   val_int(GL_MIN)},
    {"GL_MAX",                   val_int(GL_MAX)},

    /* Depth / stencil funcs */
    {"GL_NEVER",    val_int(GL_NEVER)},
    {"GL_LESS",     val_int(GL_LESS)},
    {"GL_EQUAL",    val_int(GL_EQUAL)},
    {"GL_LEQUAL",   val_int(GL_LEQUAL)},
    {"GL_GREATER",  val_int(GL_GREATER)},
    {"GL_NOTEQUAL", val_int(GL_NOTEQUAL)},
    {"GL_GEQUAL",   val_int(GL_GEQUAL)},
    {"GL_ALWAYS",   val_int(GL_ALWAYS)},

    /* Stencil ops */
    {"GL_KEEP",      val_int(GL_KEEP)},
    {"GL_REPLACE",   val_int(GL_REPLACE)},
    {"GL_INCR",      val_int(GL_INCR)},
    {"GL_DECR",      val_int(GL_DECR)},
    {"GL_INVERT",    val_int(GL_INVERT)},
    {"GL_INCR_WRAP", val_int(GL_INCR_WRAP)},
    {"GL_DECR_WRAP", val_int(GL_DECR_WRAP)},

    /* Face cull */
    {"GL_FRONT",          val_int(GL_FRONT)},
    {"GL_BACK",           val_int(GL_BACK)},
    {"GL_FRONT_AND_BACK", val_int(GL_FRONT_AND_BACK)},
    {"GL_CW",             val_int(GL_CW)},
    {"GL_CCW",            val_int(GL_CCW)},

    /* Data types */
    {"GL_BYTE",           val_int(GL_BYTE)},
    {"GL_UNSIGNED_BYTE",  val_int(GL_UNSIGNED_BYTE)},
    {"GL_SHORT",          val_int(GL_SHORT)},
    {"GL_UNSIGNED_SHORT", val_int(GL_UNSIGNED_SHORT)},
    {"GL_INT",            val_int(GL_INT)},
    {"GL_UNSIGNED_INT",   val_int(GL_UNSIGNED_INT)},
    {"GL_FLOAT",          val_int(GL_FLOAT)},
    {"GL_HALF_FLOAT",     val_int(GL_HALF_FLOAT)},

    /* Boolean */
    {"GL_TRUE",  val_int(GL_TRUE)},
    {"GL_FALSE", val_int(GL_FALSE)},

    /* Buffer targets */
    {"GL_ARRAY_BUFFER",              val_int(GL_ARRAY_BUFFER)},
    {"GL_ELEMENT_ARRAY_BUFFER",      val_int(GL_ELEMENT_ARRAY_BUFFER)},
    {"GL_UNIFORM_BUFFER",            val_int(GL_UNIFORM_BUFFER)},
    {"GL_COPY_READ_BUFFER",          val_int(GL_COPY_READ_BUFFER)},
    {"GL_COPY_WRITE_BUFFER",         val_int(GL_COPY_WRITE_BUFFER)},
    {"GL_PIXEL_PACK_BUFFER",         val_int(GL_PIXEL_PACK_BUFFER)},
    {"GL_PIXEL_UNPACK_BUFFER",       val_int(GL_PIXEL_UNPACK_BUFFER)},
    {"GL_TRANSFORM_FEEDBACK_BUFFER", val_int(GL_TRANSFORM_FEEDBACK_BUFFER)},

    /* Buffer usage */
    {"GL_STATIC_DRAW",  val_int(GL_STATIC_DRAW)},
    {"GL_DYNAMIC_DRAW", val_int(GL_DYNAMIC_DRAW)},
    {"GL_STREAM_DRAW",  val_int(GL_STREAM_DRAW)},
    {"GL_STATIC_READ",  val_int(GL_STATIC_READ)},
    {"GL_DYNAMIC_READ", val_int(GL_DYNAMIC_READ)},
    {"GL_STREAM_READ",  val_int(GL_STREAM_READ)},
    {"GL_STATIC_COPY",  val_int(GL_STATIC_COPY)},
    {"GL_DYNAMIC_COPY", val_int(GL_DYNAMIC_COPY)},
    {"GL_STREAM_COPY",  val_int(GL_STREAM_COPY)},

    /* Texture targets */
    {"GL_TEXTURE_2D",       val_int(GL_TEXTURE_2D)},
    {"GL_TEXTURE_3D",       val_int(GL_TEXTURE_3D)},
    {"GL_TEXTURE_CUBE_MAP", val_int(GL_TEXTURE_CUBE_MAP)},
    {"GL_TEXTURE_2D_ARRAY", val_int(GL_TEXTURE_2D_ARRAY)},
    {"GL_TEXTURE_CUBE_MAP_POSITIVE_X", val_int(GL_TEXTURE_CUBE_MAP_POSITIVE_X)},
    {"GL_TEXTURE_CUBE_MAP_NEGATIVE_X", val_int(GL_TEXTURE_CUBE_MAP_NEGATIVE_X)},
    {"GL_TEXTURE_CUBE_MAP_POSITIVE_Y", val_int(GL_TEXTURE_CUBE_MAP_POSITIVE_Y)},
    {"GL_TEXTURE_CUBE_MAP_NEGATIVE_Y", val_int(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y)},
    {"GL_TEXTURE_CUBE_MAP_POSITIVE_Z", val_int(GL_TEXTURE_CUBE_MAP_POSITIVE_Z)},
    {"GL_TEXTURE_CUBE_MAP_NEGATIVE_Z", val_int(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)},

    /* Texture parameters */
    {"GL_TEXTURE_MIN_FILTER", val_int(GL_TEXTURE_MIN_FILTER)},
    {"GL_TEXTURE_MAG_FILTER", val_int(GL_TEXTURE_MAG_FILTER)},
    {"GL_TEXTURE_WRAP_S",     val_int(GL_TEXTURE_WRAP_S)},
    {"GL_TEXTURE_WRAP_T",     val_int(GL_TEXTURE_WRAP_T)},
    {"GL_TEXTURE_WRAP_R",     val_int(GL_TEXTURE_WRAP_R)},
    {"GL_TEXTURE_SWIZZLE_R",    val_int(GL_TEXTURE_SWIZZLE_R)},
    {"GL_TEXTURE_SWIZZLE_G",    val_int(GL_TEXTURE_SWIZZLE_G)},
    {"GL_TEXTURE_SWIZZLE_B",    val_int(GL_TEXTURE_SWIZZLE_B)},
    {"GL_TEXTURE_SWIZZLE_A",    val_int(GL_TEXTURE_SWIZZLE_A)},
    {"GL_TEXTURE_SWIZZLE_RGBA", val_int(GL_TEXTURE_SWIZZLE_RGBA)},

    /* Texture filter modes */
    {"GL_NEAREST",                val_int(GL_NEAREST)},
    {"GL_LINEAR",                 val_int(GL_LINEAR)},
    {"GL_NEAREST_MIPMAP_NEAREST", val_int(GL_NEAREST_MIPMAP_NEAREST)},
    {"GL_LINEAR_MIPMAP_NEAREST",  val_int(GL_LINEAR_MIPMAP_NEAREST)},
    {"GL_NEAREST_MIPMAP_LINEAR",  val_int(GL_NEAREST_MIPMAP_LINEAR)},
    {"GL_LINEAR_MIPMAP_LINEAR",   val_int(GL_LINEAR_MIPMAP_LINEAR)},

    /* Texture wrap modes */
    {"GL_REPEAT",          val_int(GL_REPEAT)},
    {"GL_CLAMP_TO_EDGE",   val_int(GL_CLAMP_TO_EDGE)},
    {"GL_MIRRORED_REPEAT", val_int(GL_MIRRORED_REPEAT)},

    /* Pixel formats */
    {"GL_RED",             val_int(GL_RED)},
    {"GL_RG",              val_int(GL_RG)},
    {"GL_RGB",             val_int(GL_RGB)},
    {"GL_RGBA",            val_int(GL_RGBA)},
    {"GL_DEPTH_COMPONENT", val_int(GL_DEPTH_COMPONENT)},
    {"GL_DEPTH_STENCIL",   val_int(GL_DEPTH_STENCIL)},
    {"GL_ALPHA",           val_int(GL_ALPHA)},

    /* Sized internal formats (GLES 3.0) */
    {"GL_R8",       val_int(GL_R8)},
    {"GL_RG8",      val_int(GL_RG8)},
    {"GL_RGB8",     val_int(GL_RGB8)},
    {"GL_RGBA8",    val_int(GL_RGBA8)},
    {"GL_R16F",     val_int(GL_R16F)},
    {"GL_RG16F",    val_int(GL_RG16F)},
    {"GL_RGB16F",   val_int(GL_RGB16F)},
    {"GL_RGBA16F",  val_int(GL_RGBA16F)},
    {"GL_R32F",     val_int(GL_R32F)},
    {"GL_RG32F",    val_int(GL_RG32F)},
    {"GL_RGB32F",   val_int(GL_RGB32F)},
    {"GL_RGBA32F",  val_int(GL_RGBA32F)},
    {"GL_DEPTH_COMPONENT16",  val_int(GL_DEPTH_COMPONENT16)},
    {"GL_DEPTH_COMPONENT24",  val_int(GL_DEPTH_COMPONENT24)},
    {"GL_DEPTH_COMPONENT32F", val_int(GL_DEPTH_COMPONENT32F)},
    {"GL_DEPTH24_STENCIL8",   val_int(GL_DEPTH24_STENCIL8)},

    /* Shader types */
    {"GL_VERTEX_SHADER",   val_int(GL_VERTEX_SHADER)},
    {"GL_FRAGMENT_SHADER", val_int(GL_FRAGMENT_SHADER)},

    /* Shader query */
    {"GL_COMPILE_STATUS",     val_int(GL_COMPILE_STATUS)},
    {"GL_LINK_STATUS",        val_int(GL_LINK_STATUS)},
    {"GL_VALIDATE_STATUS",    val_int(GL_VALIDATE_STATUS)},
    {"GL_INFO_LOG_LENGTH",    val_int(GL_INFO_LOG_LENGTH)},
    {"GL_ACTIVE_ATTRIBUTES",  val_int(GL_ACTIVE_ATTRIBUTES)},
    {"GL_ACTIVE_UNIFORMS",    val_int(GL_ACTIVE_UNIFORMS)},

    /* Texture units */
    {"GL_TEXTURE0",  val_int(GL_TEXTURE0)},
    {"GL_TEXTURE1",  val_int(GL_TEXTURE0 + 1)},
    {"GL_TEXTURE2",  val_int(GL_TEXTURE0 + 2)},
    {"GL_TEXTURE3",  val_int(GL_TEXTURE0 + 3)},
    {"GL_TEXTURE4",  val_int(GL_TEXTURE0 + 4)},
    {"GL_TEXTURE5",  val_int(GL_TEXTURE0 + 5)},
    {"GL_TEXTURE6",  val_int(GL_TEXTURE0 + 6)},
    {"GL_TEXTURE7",  val_int(GL_TEXTURE0 + 7)},
    {"GL_TEXTURE8",  val_int(GL_TEXTURE0 + 8)},
    {"GL_TEXTURE9",  val_int(GL_TEXTURE0 + 9)},
    {"GL_TEXTURE10", val_int(GL_TEXTURE0 + 10)},
    {"GL_TEXTURE11", val_int(GL_TEXTURE0 + 11)},
    {"GL_TEXTURE12", val_int(GL_TEXTURE0 + 12)},
    {"GL_TEXTURE13", val_int(GL_TEXTURE0 + 13)},
    {"GL_TEXTURE14", val_int(GL_TEXTURE0 + 14)},
    {"GL_TEXTURE15", val_int(GL_TEXTURE0 + 15)},

    /* Uniform types (glGetActiveUniform) */
    {"GL_FLOAT_VEC2",   val_int(GL_FLOAT_VEC2)},
    {"GL_FLOAT_VEC3",   val_int(GL_FLOAT_VEC3)},
    {"GL_FLOAT_VEC4",   val_int(GL_FLOAT_VEC4)},
    {"GL_INT_VEC2",     val_int(GL_INT_VEC2)},
    {"GL_INT_VEC3",     val_int(GL_INT_VEC3)},
    {"GL_INT_VEC4",     val_int(GL_INT_VEC4)},
    {"GL_BOOL",         val_int(GL_BOOL)},
    {"GL_FLOAT_MAT2",   val_int(GL_FLOAT_MAT2)},
    {"GL_FLOAT_MAT3",   val_int(GL_FLOAT_MAT3)},
    {"GL_FLOAT_MAT4",   val_int(GL_FLOAT_MAT4)},
    {"GL_SAMPLER_2D",   val_int(GL_SAMPLER_2D)},
    {"GL_SAMPLER_CUBE", val_int(GL_SAMPLER_CUBE)},
    {"GL_SAMPLER_3D",   val_int(GL_SAMPLER_3D)},

    /* FBO targets/attachments */
    {"GL_FRAMEBUFFER",       val_int(GL_FRAMEBUFFER)},
    {"GL_READ_FRAMEBUFFER",  val_int(GL_READ_FRAMEBUFFER)},
    {"GL_DRAW_FRAMEBUFFER",  val_int(GL_DRAW_FRAMEBUFFER)},
    {"GL_RENDERBUFFER",      val_int(GL_RENDERBUFFER)},
    {"GL_COLOR_ATTACHMENT0",  val_int(GL_COLOR_ATTACHMENT0)},
    {"GL_COLOR_ATTACHMENT1",  val_int(GL_COLOR_ATTACHMENT0 + 1)},
    {"GL_COLOR_ATTACHMENT2",  val_int(GL_COLOR_ATTACHMENT0 + 2)},
    {"GL_COLOR_ATTACHMENT3",  val_int(GL_COLOR_ATTACHMENT0 + 3)},
    {"GL_DEPTH_ATTACHMENT",   val_int(GL_DEPTH_ATTACHMENT)},
    {"GL_STENCIL_ATTACHMENT", val_int(GL_STENCIL_ATTACHMENT)},
    {"GL_DEPTH_STENCIL_ATTACHMENT", val_int(GL_DEPTH_STENCIL_ATTACHMENT)},
    {"GL_FRAMEBUFFER_COMPLETE", val_int(GL_FRAMEBUFFER_COMPLETE)},

    /* Pixel store */
    {"GL_PACK_ALIGNMENT",   val_int(GL_PACK_ALIGNMENT)},
    {"GL_UNPACK_ALIGNMENT", val_int(GL_UNPACK_ALIGNMENT)},

    /* GetString */
    {"GL_VENDOR",                   val_int(GL_VENDOR)},
    {"GL_RENDERER",                 val_int(GL_RENDERER)},
    {"GL_VERSION",                  val_int(GL_VERSION)},
    {"GL_SHADING_LANGUAGE_VERSION", val_int(GL_SHADING_LANGUAGE_VERSION)},
    {"GL_EXTENSIONS",               val_int(GL_EXTENSIONS)},

    /* Error codes */
    {"GL_NO_ERROR",          val_int(GL_NO_ERROR)},
    {"GL_INVALID_ENUM",      val_int(GL_INVALID_ENUM)},
    {"GL_INVALID_VALUE",     val_int(GL_INVALID_VALUE)},
    {"GL_INVALID_OPERATION", val_int(GL_INVALID_OPERATION)},
    {"GL_OUT_OF_MEMORY",     val_int(GL_OUT_OF_MEMORY)},

    /* UBO (GLES 3.0) */
    {"GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT", val_int(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT)},
    {"GL_MAX_UNIFORM_BLOCK_SIZE",          val_int(GL_MAX_UNIFORM_BLOCK_SIZE)},

    /* Buffer mapping flags */
    {"GL_MAP_READ_BIT",                val_int(GL_MAP_READ_BIT)},
    {"GL_MAP_WRITE_BIT",               val_int(GL_MAP_WRITE_BIT)},
    {"GL_MAP_INVALIDATE_RANGE_BIT",    val_int(GL_MAP_INVALIDATE_RANGE_BIT)},
    {"GL_MAP_INVALIDATE_BUFFER_BIT",   val_int(GL_MAP_INVALIDATE_BUFFER_BIT)},
    {"GL_MAP_FLUSH_EXPLICIT_BIT",      val_int(GL_MAP_FLUSH_EXPLICIT_BIT)},
    {"GL_MAP_UNSYNCHRONIZED_BIT",      val_int(GL_MAP_UNSYNCHRONIZED_BIT)},
    {"GL_BUFFER_MAPPED",               val_int(GL_BUFFER_MAPPED)},
    {"GL_BUFFER_MAP_POINTER",          val_int(GL_BUFFER_MAP_POINTER)},
    {"GL_VERTEX_ATTRIB_ARRAY_POINTER", val_int(GL_VERTEX_ATTRIB_ARRAY_POINTER)},

    /* Sync objects */
    {"GL_SYNC_GPU_COMMANDS_COMPLETE", val_int(GL_SYNC_GPU_COMMANDS_COMPLETE)},
    {"GL_ALREADY_SIGNALED",           val_int(GL_ALREADY_SIGNALED)},
    {"GL_TIMEOUT_EXPIRED",            val_int(GL_TIMEOUT_EXPIRED)},
    {"GL_CONDITION_SATISFIED",        val_int(GL_CONDITION_SATISFIED)},
    {"GL_WAIT_FAILED",                val_int(GL_WAIT_FAILED)},
    {"GL_SYNC_FLUSH_COMMANDS_BIT",    val_int(GL_SYNC_FLUSH_COMMANDS_BIT)},
    /* GL_TIMEOUT_IGNORED is 0xFFFFFFFFFFFFFFFFull — too large for int; expose as -1 sentinel */
    {"GL_TIMEOUT_IGNORED",            val_int(-1)},

    /* Queries */
    {"GL_ANY_SAMPLES_PASSED",                      val_int(GL_ANY_SAMPLES_PASSED)},
    {"GL_ANY_SAMPLES_PASSED_CONSERVATIVE",         val_int(GL_ANY_SAMPLES_PASSED_CONSERVATIVE)},
    {"GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN",   val_int(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN)},
    {"GL_QUERY_RESULT",                            val_int(GL_QUERY_RESULT)},
    {"GL_QUERY_RESULT_AVAILABLE",                  val_int(GL_QUERY_RESULT_AVAILABLE)},

    /* Transform Feedback */
    {"GL_INTERLEAVED_ATTRIBS",           val_int(GL_INTERLEAVED_ATTRIBS)},
    {"GL_SEPARATE_ATTRIBS",              val_int(GL_SEPARATE_ATTRIBS)},
    {"GL_TRANSFORM_FEEDBACK",            val_int(GL_TRANSFORM_FEEDBACK)},
    {"GL_TRANSFORM_FEEDBACK_ACTIVE",     val_int(GL_TRANSFORM_FEEDBACK_ACTIVE)},
    {"GL_TRANSFORM_FEEDBACK_PAUSED",     val_int(GL_TRANSFORM_FEEDBACK_PAUSED)},
    {"GL_RASTERIZER_DISCARD",            val_int(GL_RASTERIZER_DISCARD)},

    /* UBO introspection */
    {"GL_UNIFORM_TYPE",                   val_int(GL_UNIFORM_TYPE)},
    {"GL_UNIFORM_SIZE",                   val_int(GL_UNIFORM_SIZE)},
    {"GL_UNIFORM_NAME_LENGTH",            val_int(GL_UNIFORM_NAME_LENGTH)},
    {"GL_UNIFORM_BLOCK_INDEX",            val_int(GL_UNIFORM_BLOCK_INDEX)},
    {"GL_UNIFORM_OFFSET",                 val_int(GL_UNIFORM_OFFSET)},
    {"GL_UNIFORM_ARRAY_STRIDE",           val_int(GL_UNIFORM_ARRAY_STRIDE)},
    {"GL_UNIFORM_MATRIX_STRIDE",          val_int(GL_UNIFORM_MATRIX_STRIDE)},
    {"GL_UNIFORM_IS_ROW_MAJOR",           val_int(GL_UNIFORM_IS_ROW_MAJOR)},
    {"GL_UNIFORM_BLOCK_BINDING",          val_int(GL_UNIFORM_BLOCK_BINDING)},
    {"GL_UNIFORM_BLOCK_DATA_SIZE",        val_int(GL_UNIFORM_BLOCK_DATA_SIZE)},
    {"GL_MAX_VERTEX_UNIFORM_BLOCKS",      val_int(GL_MAX_VERTEX_UNIFORM_BLOCKS)},
    {"GL_MAX_FRAGMENT_UNIFORM_BLOCKS",    val_int(GL_MAX_FRAGMENT_UNIFORM_BLOCKS)},
    {"GL_MAX_COMBINED_UNIFORM_BLOCKS",    val_int(GL_MAX_COMBINED_UNIFORM_BLOCKS)},
    {"GL_INVALID_INDEX",                  val_int((int)GL_INVALID_INDEX)},

    /* Pixel Store ES3 additions */
    {"GL_UNPACK_ROW_LENGTH",    val_int(GL_UNPACK_ROW_LENGTH)},
    {"GL_UNPACK_IMAGE_HEIGHT",  val_int(GL_UNPACK_IMAGE_HEIGHT)},
    {"GL_UNPACK_SKIP_ROWS",     val_int(GL_UNPACK_SKIP_ROWS)},
    {"GL_UNPACK_SKIP_PIXELS",   val_int(GL_UNPACK_SKIP_PIXELS)},
    {"GL_UNPACK_SKIP_IMAGES",   val_int(GL_UNPACK_SKIP_IMAGES)},
    {"GL_PACK_ROW_LENGTH",      val_int(GL_PACK_ROW_LENGTH)},
    {"GL_PACK_SKIP_ROWS",       val_int(GL_PACK_SKIP_ROWS)},
    {"GL_PACK_SKIP_PIXELS",     val_int(GL_PACK_SKIP_PIXELS)},

    /* Uniform types — bool vectors */
    {"GL_BOOL_VEC2", val_int(GL_BOOL_VEC2)},
    {"GL_BOOL_VEC3", val_int(GL_BOOL_VEC3)},
    {"GL_BOOL_VEC4", val_int(GL_BOOL_VEC4)},
    /* Uniform types — uint vectors */
    {"GL_UNSIGNED_INT_VEC2", val_int(GL_UNSIGNED_INT_VEC2)},
    {"GL_UNSIGNED_INT_VEC3", val_int(GL_UNSIGNED_INT_VEC3)},
    {"GL_UNSIGNED_INT_VEC4", val_int(GL_UNSIGNED_INT_VEC4)},
    /* Uniform types — non-square matrices */
    {"GL_FLOAT_MAT2x3", val_int(GL_FLOAT_MAT2x3)},
    {"GL_FLOAT_MAT3x2", val_int(GL_FLOAT_MAT3x2)},
    {"GL_FLOAT_MAT2x4", val_int(GL_FLOAT_MAT2x4)},
    {"GL_FLOAT_MAT4x2", val_int(GL_FLOAT_MAT4x2)},
    {"GL_FLOAT_MAT3x4", val_int(GL_FLOAT_MAT3x4)},
    {"GL_FLOAT_MAT4x3", val_int(GL_FLOAT_MAT4x3)},
    /* Uniform types — shadow/array samplers */
    {"GL_SAMPLER_2D_SHADOW",        val_int(GL_SAMPLER_2D_SHADOW)},
    {"GL_SAMPLER_2D_ARRAY",         val_int(GL_SAMPLER_2D_ARRAY)},
    {"GL_SAMPLER_2D_ARRAY_SHADOW",  val_int(GL_SAMPLER_2D_ARRAY_SHADOW)},
    {"GL_SAMPLER_CUBE_SHADOW",      val_int(GL_SAMPLER_CUBE_SHADOW)},
    /* Uniform types — int samplers */
    {"GL_INT_SAMPLER_2D",           val_int(GL_INT_SAMPLER_2D)},
    {"GL_INT_SAMPLER_3D",           val_int(GL_INT_SAMPLER_3D)},
    {"GL_INT_SAMPLER_CUBE",         val_int(GL_INT_SAMPLER_CUBE)},
    {"GL_INT_SAMPLER_2D_ARRAY",     val_int(GL_INT_SAMPLER_2D_ARRAY)},
    /* Uniform types — uint samplers */
    {"GL_UNSIGNED_INT_SAMPLER_2D",       val_int(GL_UNSIGNED_INT_SAMPLER_2D)},
    {"GL_UNSIGNED_INT_SAMPLER_3D",       val_int(GL_UNSIGNED_INT_SAMPLER_3D)},
    {"GL_UNSIGNED_INT_SAMPLER_CUBE",     val_int(GL_UNSIGNED_INT_SAMPLER_CUBE)},
    {"GL_UNSIGNED_INT_SAMPLER_2D_ARRAY", val_int(GL_UNSIGNED_INT_SAMPLER_2D_ARRAY)},

    /* Sized integer internal formats */
    {"GL_R8I",     val_int(GL_R8I)},
    {"GL_R8UI",    val_int(GL_R8UI)},
    {"GL_R16I",    val_int(GL_R16I)},
    {"GL_R16UI",   val_int(GL_R16UI)},
    {"GL_R32I",    val_int(GL_R32I)},
    {"GL_R32UI",   val_int(GL_R32UI)},
    {"GL_RG8I",    val_int(GL_RG8I)},
    {"GL_RG8UI",   val_int(GL_RG8UI)},
    {"GL_RG16I",   val_int(GL_RG16I)},
    {"GL_RG16UI",  val_int(GL_RG16UI)},
    {"GL_RG32I",   val_int(GL_RG32I)},
    {"GL_RG32UI",  val_int(GL_RG32UI)},
    {"GL_RGBA8I",   val_int(GL_RGBA8I)},
    {"GL_RGBA8UI",  val_int(GL_RGBA8UI)},
    {"GL_RGBA16I",  val_int(GL_RGBA16I)},
    {"GL_RGBA16UI", val_int(GL_RGBA16UI)},
    {"GL_RGBA32I",  val_int(GL_RGBA32I)},
    {"GL_RGBA32UI", val_int(GL_RGBA32UI)},
    {"GL_RGB10_A2",    val_int(GL_RGB10_A2)},
    {"GL_RGB10_A2UI",  val_int(GL_RGB10_A2UI)},
    {"GL_R11F_G11F_B10F", val_int(GL_R11F_G11F_B10F)},
    {"GL_RGB9_E5",     val_int(GL_RGB9_E5)},
    {"GL_SRGB8",          val_int(GL_SRGB8)},
    {"GL_SRGB8_ALPHA8",   val_int(GL_SRGB8_ALPHA8)},

    /* FBO status codes */
    {"GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT",         val_int(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)},
    {"GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT", val_int(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)},
    {"GL_FRAMEBUFFER_UNSUPPORTED",                   val_int(GL_FRAMEBUFFER_UNSUPPORTED)},
    {"GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE",        val_int(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)},
    {"GL_INVALID_FRAMEBUFFER_OPERATION",             val_int(GL_INVALID_FRAMEBUFFER_OPERATION)},

    /* Program Binary */
    {"GL_PROGRAM_BINARY_RETRIEVABLE_HINT", val_int(GL_PROGRAM_BINARY_RETRIEVABLE_HINT)},
    {"GL_NUM_PROGRAM_BINARY_FORMATS",      val_int(GL_NUM_PROGRAM_BINARY_FORMATS)},
    {"GL_PROGRAM_BINARY_FORMATS",          val_int(GL_PROGRAM_BINARY_FORMATS)},
    {"GL_PROGRAM_BINARY_LENGTH",           val_int(GL_PROGRAM_BINARY_LENGTH)},

    /* GetStringi */
    {"GL_NUM_EXTENSIONS", val_int(GL_NUM_EXTENSIONS)},
};
