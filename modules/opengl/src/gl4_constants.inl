/* gl4_constants.inl — Desktop GL 4.x constants
** Included by zen_gl4.cpp */

static const NativeConst gl4_constants[] = {
    /* Tessellation */
    {"GL_PATCHES",                    val_int(GL_PATCHES)},
    {"GL_PATCH_VERTICES",             val_int(GL_PATCH_VERTICES)},
    {"GL_PATCH_DEFAULT_INNER_LEVEL",  val_int(GL_PATCH_DEFAULT_INNER_LEVEL)},
    {"GL_PATCH_DEFAULT_OUTER_LEVEL",  val_int(GL_PATCH_DEFAULT_OUTER_LEVEL)},
    {"GL_TESS_CONTROL_SHADER",        val_int(GL_TESS_CONTROL_SHADER)},
    {"GL_TESS_EVALUATION_SHADER",     val_int(GL_TESS_EVALUATION_SHADER)},
    {"GL_MAX_PATCH_VERTICES",         val_int(GL_MAX_PATCH_VERTICES)},
    {"GL_MAX_TESS_GEN_LEVEL",         val_int(GL_MAX_TESS_GEN_LEVEL)},

    /* Geometry shader */
    {"GL_GEOMETRY_SHADER",            val_int(GL_GEOMETRY_SHADER)},
    {"GL_MAX_GEOMETRY_OUTPUT_VERTICES", val_int(GL_MAX_GEOMETRY_OUTPUT_VERTICES)},

    /* Compute shader */
    {"GL_COMPUTE_SHADER",             val_int(GL_COMPUTE_SHADER)},
    {"GL_MAX_COMPUTE_WORK_GROUP_COUNT", val_int(GL_MAX_COMPUTE_WORK_GROUP_COUNT)},
    {"GL_MAX_COMPUTE_WORK_GROUP_SIZE",  val_int(GL_MAX_COMPUTE_WORK_GROUP_SIZE)},
    {"GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS", val_int(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS)},

    /* Image load/store */
    {"GL_READ_ONLY",                  val_int(GL_READ_ONLY)},
    {"GL_WRITE_ONLY",                 val_int(GL_WRITE_ONLY)},
    {"GL_READ_WRITE",                 val_int(GL_READ_WRITE)},
    {"GL_ALL_BARRIER_BITS",           val_int(GL_ALL_BARRIER_BITS)},
    {"GL_SHADER_IMAGE_ACCESS_BARRIER_BIT", val_int(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT)},
    {"GL_SHADER_STORAGE_BARRIER_BIT", val_int(GL_SHADER_STORAGE_BARRIER_BIT)},
    {"GL_UNIFORM_BARRIER_BIT",        val_int(GL_UNIFORM_BARRIER_BIT)},
    {"GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT", val_int(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT)},
    {"GL_ELEMENT_ARRAY_BARRIER_BIT",  val_int(GL_ELEMENT_ARRAY_BARRIER_BIT)},
    {"GL_TEXTURE_FETCH_BARRIER_BIT",  val_int(GL_TEXTURE_FETCH_BARRIER_BIT)},
    {"GL_TEXTURE_UPDATE_BARRIER_BIT", val_int(GL_TEXTURE_UPDATE_BARRIER_BIT)},
    {"GL_BUFFER_UPDATE_BARRIER_BIT",  val_int(GL_BUFFER_UPDATE_BARRIER_BIT)},
    {"GL_FRAMEBUFFER_BARRIER_BIT",    val_int(GL_FRAMEBUFFER_BARRIER_BIT)},
    {"GL_TRANSFORM_FEEDBACK_BARRIER_BIT", val_int(GL_TRANSFORM_FEEDBACK_BARRIER_BIT)},
    {"GL_ATOMIC_COUNTER_BARRIER_BIT", val_int(GL_ATOMIC_COUNTER_BARRIER_BIT)},

    /* Multisample textures */
    {"GL_TEXTURE_2D_MULTISAMPLE",       val_int(GL_TEXTURE_2D_MULTISAMPLE)},
    {"GL_TEXTURE_2D_MULTISAMPLE_ARRAY", val_int(GL_TEXTURE_2D_MULTISAMPLE_ARRAY)},
    {"GL_SAMPLE_POSITION",              val_int(GL_SAMPLE_POSITION)},
    {"GL_SAMPLE_MASK",                  val_int(GL_SAMPLE_MASK)},
    {"GL_SAMPLE_MASK_VALUE",            val_int(GL_SAMPLE_MASK_VALUE)},
    {"GL_MAX_SAMPLES",                  val_int(GL_MAX_SAMPLES)},
    {"GL_MAX_COLOR_TEXTURE_SAMPLES",    val_int(GL_MAX_COLOR_TEXTURE_SAMPLES)},
    {"GL_MAX_DEPTH_TEXTURE_SAMPLES",    val_int(GL_MAX_DEPTH_TEXTURE_SAMPLES)},
    {"GL_MAX_INTEGER_SAMPLES",          val_int(GL_MAX_INTEGER_SAMPLES)},

    /* Buffer storage flags */
    {"GL_MAP_READ_BIT",               val_int(GL_MAP_READ_BIT)},
    {"GL_MAP_WRITE_BIT",              val_int(GL_MAP_WRITE_BIT)},
    {"GL_MAP_PERSISTENT_BIT",         val_int(GL_MAP_PERSISTENT_BIT)},
    {"GL_MAP_COHERENT_BIT",           val_int(GL_MAP_COHERENT_BIT)},
    {"GL_DYNAMIC_STORAGE_BIT",        val_int(GL_DYNAMIC_STORAGE_BIT)},
    {"GL_CLIENT_STORAGE_BIT",         val_int(GL_CLIENT_STORAGE_BIT)},
    {"GL_MAP_FLUSH_EXPLICIT_BIT",     val_int(GL_MAP_FLUSH_EXPLICIT_BIT)},
    {"GL_MAP_UNSYNCHRONIZED_BIT",     val_int(GL_MAP_UNSYNCHRONIZED_BIT)},
    {"GL_MAP_INVALIDATE_RANGE_BIT",   val_int(GL_MAP_INVALIDATE_RANGE_BIT)},
    {"GL_MAP_INVALIDATE_BUFFER_BIT",  val_int(GL_MAP_INVALIDATE_BUFFER_BIT)},

    /* Timer queries */
    {"GL_TIME_ELAPSED",               val_int(GL_TIME_ELAPSED)},
    {"GL_TIMESTAMP",                  val_int(GL_TIMESTAMP)},
    {"GL_QUERY_RESULT",               val_int(GL_QUERY_RESULT)},
    {"GL_QUERY_RESULT_AVAILABLE",     val_int(GL_QUERY_RESULT_AVAILABLE)},

    /* SSBO */
    {"GL_SHADER_STORAGE_BUFFER",      val_int(GL_SHADER_STORAGE_BUFFER)},
    {"GL_MAX_SHADER_STORAGE_BLOCK_SIZE", val_int(GL_MAX_SHADER_STORAGE_BLOCK_SIZE)},
    {"GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS", val_int(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS)},

    /* Vertex attrib binding */
    {"GL_VERTEX_ATTRIB_BINDING",      val_int(GL_VERTEX_ATTRIB_BINDING)},
    {"GL_VERTEX_ATTRIB_RELATIVE_OFFSET", val_int(GL_VERTEX_ATTRIB_RELATIVE_OFFSET)},
    {"GL_VERTEX_BINDING_DIVISOR",     val_int(GL_VERTEX_BINDING_DIVISOR)},
    {"GL_VERTEX_BINDING_OFFSET",      val_int(GL_VERTEX_BINDING_OFFSET)},
    {"GL_VERTEX_BINDING_STRIDE",      val_int(GL_VERTEX_BINDING_STRIDE)},

    /* Polygon modes */
    {"GL_POINT",                      val_int(GL_POINT)},
    {"GL_LINE",                       val_int(GL_LINE)},
    {"GL_FILL",                       val_int(GL_FILL)},

    /* Logic op */
    {"GL_CLEAR",                      val_int(GL_CLEAR)},
    {"GL_SET",                        val_int(GL_SET)},
    {"GL_COPY",                       val_int(GL_COPY)},
    {"GL_COPY_INVERTED",              val_int(GL_COPY_INVERTED)},
    {"GL_NOOP",                       val_int(GL_NOOP)},
    {"GL_INVERT",                     val_int(GL_INVERT)},
    {"GL_AND",                        val_int(GL_AND)},
    {"GL_NAND",                       val_int(GL_NAND)},
    {"GL_OR",                         val_int(GL_OR)},
    {"GL_NOR",                        val_int(GL_NOR)},
    {"GL_XOR",                        val_int(GL_XOR)},
    {"GL_EQUIV",                      val_int(GL_EQUIV)},
    {"GL_AND_REVERSE",                val_int(GL_AND_REVERSE)},
    {"GL_AND_INVERTED",               val_int(GL_AND_INVERTED)},
    {"GL_OR_REVERSE",                 val_int(GL_OR_REVERSE)},
    {"GL_OR_INVERTED",                val_int(GL_OR_INVERTED)},
    {"GL_COLOR_LOGIC_OP",             val_int(GL_COLOR_LOGIC_OP)},

    /* Debug output */
    {"GL_DEBUG_OUTPUT",               val_int(GL_DEBUG_OUTPUT)},
    {"GL_DEBUG_OUTPUT_SYNCHRONOUS",   val_int(GL_DEBUG_OUTPUT_SYNCHRONOUS)},
    {"GL_DEBUG_SOURCE_API",           val_int(GL_DEBUG_SOURCE_API)},
    {"GL_DEBUG_SOURCE_APPLICATION",   val_int(GL_DEBUG_SOURCE_APPLICATION)},
    {"GL_DEBUG_SOURCE_OTHER",         val_int(GL_DEBUG_SOURCE_OTHER)},
    {"GL_DEBUG_TYPE_ERROR",           val_int(GL_DEBUG_TYPE_ERROR)},
    {"GL_DEBUG_TYPE_PERFORMANCE",     val_int(GL_DEBUG_TYPE_PERFORMANCE)},
    {"GL_DEBUG_TYPE_OTHER",           val_int(GL_DEBUG_TYPE_OTHER)},
    {"GL_DEBUG_TYPE_MARKER",          val_int(GL_DEBUG_TYPE_MARKER)},
    {"GL_DEBUG_TYPE_PUSH_GROUP",      val_int(GL_DEBUG_TYPE_PUSH_GROUP)},
    {"GL_DEBUG_TYPE_POP_GROUP",       val_int(GL_DEBUG_TYPE_POP_GROUP)},
    {"GL_DEBUG_SEVERITY_HIGH",        val_int(GL_DEBUG_SEVERITY_HIGH)},
    {"GL_DEBUG_SEVERITY_MEDIUM",      val_int(GL_DEBUG_SEVERITY_MEDIUM)},
    {"GL_DEBUG_SEVERITY_LOW",         val_int(GL_DEBUG_SEVERITY_LOW)},
    {"GL_DEBUG_SEVERITY_NOTIFICATION",val_int(GL_DEBUG_SEVERITY_NOTIFICATION)},
    {"GL_DONT_CARE",                  val_int(GL_DONT_CARE)},

    /* Framebuffer no-attachment params */
    {"GL_FRAMEBUFFER_DEFAULT_WIDTH",  val_int(GL_FRAMEBUFFER_DEFAULT_WIDTH)},
    {"GL_FRAMEBUFFER_DEFAULT_HEIGHT", val_int(GL_FRAMEBUFFER_DEFAULT_HEIGHT)},
    {"GL_FRAMEBUFFER_DEFAULT_LAYERS", val_int(GL_FRAMEBUFFER_DEFAULT_LAYERS)},
    {"GL_FRAMEBUFFER_DEFAULT_SAMPLES",val_int(GL_FRAMEBUFFER_DEFAULT_SAMPLES)},
    {"GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS", val_int(GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS)},
};
