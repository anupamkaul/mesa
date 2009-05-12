
# this isn't really needed, but informative
MAIN_OMITTED = \
	main/accum.c \
	main/accum.h \
	main/attrib.c \
	main/attrib.h \
	main/api_exec.c \
	main/colortab.c \
	main/colortab.h \
	main/convolve.c \
	main/convolve.h \
	main/dispatch.c \
	main/dlist.c \
	main/dlist.h \
	main/drawpix.c \
	main/drawpix.h \
	main/eval.c \
	main/eval.h \
	main/feedback.c \
	main/feedback.h \
	main/get.c \
	main/histogram.c \
	main/histogram.h \
	main/mm.c \
	main/queryobj.c \
	main/queryobj.h \
	main/rastpos.c \
	main/rastpos.h \
	main/texcompress_fxt1.c \
	main/texcompress_s3tc.c


# main/*.c symlinked
MAIN_C_LINKS = \
	main/api_arrayelt.c \
	main/api_loopback.c \
	main/api_noop.c \
	main/api_validate.c \
	main/arrayobj.c \
	main/blend.c \
	main/bufferobj.c \
	main/buffers.c \
	main/clear.c \
	main/clip.c \
	main/context.c \
	main/debug.c \
	main/depth.c \
	main/depthstencil.c \
	main/enable.c \
	main/enums.c \
	main/execmem.c \
	main/extensions.c \
	main/fbobject.c \
	main/ffvertex_prog.c \
	main/fog.c \
	main/framebuffer.c \
	main/getstring.c \
	main/hash.c \
	main/hint.c \
	main/image.c \
	main/imports.c \
	main/light.c \
	main/lines.c \
	main/matrix.c \
	main/mipmap.c \
	main/multisample.c \
	main/pixelstore.c \
	main/points.c \
	main/polygon.c \
	main/rbadaptors.c \
	main/readpix.c \
	main/renderbuffer.c \
	main/scissor.c \
	main/shaders.c \
	main/state.c \
	main/stencil.c \
	main/texcompress.c \
	main/texenv.c \
	main/texenvprogram.c \
	main/texgen.c \
	main/texformat.c \
	main/teximage.c \
	main/texobj.c \
	main/texparam.c \
	main/texrender.c \
	main/texstate.c \
	main/texstore.c \
	main/varray.c \
	main/vsnprintf.c \
	main/vtxfmt.c

# main/*.h symlinked
MAIN_H_LINKS = \
	main/api_arrayelt.h \
	main/api_exec.h \
	main/api_loopback.h \
	main/api_noop.h \
	main/api_validate.h \
	main/arrayobj.h \
	main/bitset.h \
	main/blend.h \
	main/bufferobj.h \
	main/buffers.h \
	main/clear.h \
	main/clip.h \
	main/colormac.h \
	main/config.h \
	main/context.h \
	main/dd.h \
	main/debug.h \
	main/depth.h \
	main/depthstencil.h \
	main/enable.h \
	main/enums.h \
	main/extensions.h \
	main/fbobject.h \
	main/ffvertex_prog.h \
	main/fog.h \
	main/framebuffer.h \
	main/get.h \
	main/glheader.h \
	main/hash.h \
	main/hint.h \
	main/image.h \
	main/imports.h \
	main/light.h \
	main/lines.h \
	main/macros.h \
	main/matrix.h \
	main/mipmap.h \
	main/mm.h \
	main/mtypes.h \
	main/multisample.h \
	main/pixel.h \
	main/pixelstore.h \
	main/points.h \
	main/polygon.h \
	main/rbadaptors.h \
	main/readpix.h \
	main/renderbuffer.h \
	main/scissor.h \
	main/shaders.h \
	main/simple_list.h \
	main/state.h \
	main/stencil.h \
	main/texcompress.h \
	main/texenv.h \
	main/texenvprogram.h \
	main/texformat.h \
	main/texformat_tmp.h \
	main/texgen.h \
	main/teximage.h \
	main/texobj.h \
	main/texparam.h \
	main/texrender.h \
	main/texstate.h \
	main/texstore.h \
	main/varray.h \
	main/version.h \
	main/vtxfmt.h \
	main/vtxfmt_tmp.h


GLAPI_C_LINKS = \
	glapi/glthread.c

GLAPI_H_LINKS = \
	glapi/dispatch.h \
	glapi/glapi.h \
	glapi/glapioffsets.h \
	glapi/glapitable.h \
	glapi/glapitemp.h \
	glapi/glprocs.h \
	glapi/glthread.h


MATH_C_LINKS = \
	math/m_debug_clip.c \
	math/m_debug_norm.c \
	math/m_debug_xform.c \
	math/m_eval.c \
	math/m_matrix.c \
	math/m_translate.c \
	math/m_vector.c \
	math/m_xform.c

MATH_H_LINKS = \
	math/mathmod.h \
	math/m_clip_tmp.h \
	math/m_copy_tmp.h \
	math/m_debug.h \
	math/m_debug_util.h \
	math/m_dotprod_tmp.h \
	math/m_eval.h \
	math/m_matrix.h \
	math/m_norm_tmp.h \
	math/m_translate.h \
	math/m_trans_tmp.h \
	math/m_vector.h \
	math/m_xform.h \
	math/m_xform_tmp.h


VBO_C_LINKS = \
	vbo/vbo_context.c \
	vbo/vbo_exec_api.c \
	vbo/vbo_exec_array.c \
	vbo/vbo_exec.c \
	vbo/vbo_exec_draw.c \
	vbo/vbo_exec_eval.c \
	vbo/vbo_rebase.c \
	vbo/vbo_split.c \
	vbo/vbo_split_copy.c \
	vbo/vbo_split_inplace.c

VBO_H_LINKS = \
	vbo/vbo_attrib.h \
	vbo/vbo_attrib_tmp.h \
	vbo/vbo_context.h \
	vbo/vbo_exec_array.c \
	vbo/vbo_exec.h \
	vbo/vbo.h \
	vbo/vbo_split.h

SHADER_C_LINKS = \
	shader/arbprogparse.c \
	shader/arbprogram.c \
	shader/prog_cache.c \
	shader/prog_execute.c \
	shader/prog_instruction.c \
	shader/prog_parameter.c \
	shader/prog_print.c \
	shader/program.c \
	shader/programopt.c \
	shader/prog_statevars.c \
	shader/prog_uniform.c \
	shader/shader_api.c \
	shader/grammar/grammar_mesa.c \
	shader/slang/slang_builtin.c \
	shader/slang/slang_codegen.c \
	shader/slang/slang_compile.c \
	shader/slang/slang_compile_function.c \
	shader/slang/slang_compile_operation.c \
	shader/slang/slang_compile_struct.c \
	shader/slang/slang_compile_variable.c \
	shader/slang/slang_emit.c \
	shader/slang/slang_ir.c \
	shader/slang/slang_label.c \
	shader/slang/slang_library_noise.c \
	shader/slang/slang_link.c \
	shader/slang/slang_log.c \
	shader/slang/slang_mem.c \
	shader/slang/slang_preprocess.c \
	shader/slang/slang_print.c \
	shader/slang/slang_simplify.c \
	shader/slang/slang_storage.c \
	shader/slang/slang_typeinfo.c \
	shader/slang/slang_utility.c \
	shader/slang/slang_vartable.c \

SHADER_H_LINKS = \
	shader/arbprogparse.h \
	shader/arbprogram.h \
	shader/arbprogram_syn.h \
	shader/prog_cache.h \
	shader/prog_execute.h \
	shader/prog_instruction.h \
	shader/prog_parameter.h \
	shader/prog_print.h \
	shader/program.h \
	shader/programopt.h \
	shader/prog_statevars.h \
	shader/prog_uniform.h \
	shader/shader_api.h \
	shader/grammar/grammar_crt.h \
	shader/grammar/grammar.h \
	shader/grammar/grammar_mesa.h \
	shader/grammar/grammar_syn.h \
	shader/slang/slang_builtin.h \
	shader/slang/slang_codegen.h \
	shader/slang/slang_compile_function.h \
	shader/slang/slang_compile.h \
	shader/slang/slang_compile_operation.h \
	shader/slang/slang_compile_struct.h \
	shader/slang/slang_compile_variable.h \
	shader/slang/slang_emit.h \
	shader/slang/slang_ir.h \
	shader/slang/slang_label.h \
	shader/slang/slang_library_noise.h \
	shader/slang/slang_link.h \
	shader/slang/slang_log.h \
	shader/slang/slang_mem.h \
	shader/slang/slang_preprocess.h \
	shader/slang/slang_print.h \
	shader/slang/slang_simplify.h \
	shader/slang/slang_storage.h \
	shader/slang/slang_typeinfo.h \
	shader/slang/slang_utility.h \
	shader/slang/slang_vartable.h \
	shader/slang/library/slang_120_core_gc.h \
	shader/slang/library/slang_builtin_120_common_gc.h \
	shader/slang/library/slang_builtin_120_fragment_gc.h \
	shader/slang/library/slang_common_builtin_gc.h \
	shader/slang/library/slang_core_gc.h \
	shader/slang/library/slang_fragment_builtin_gc.h \
	shader/slang/library/slang_pp_directives_syn.h \
	shader/slang/library/slang_pp_expression_syn.h \
	shader/slang/library/slang_pp_version_syn.h \
	shader/slang/library/slang_shader_syn.h \
	shader/slang/library/slang_vertex_builtin_gc.h \


STATE_TRACKER_C_LINKS = \
	state_tracker/st_atom_blend.c \
	state_tracker/st_atom.c \
	state_tracker/st_atom_clip.c \
	state_tracker/st_atom_constbuf.c \
	state_tracker/st_atom_depth.c \
	state_tracker/st_atom_framebuffer.c \
	state_tracker/st_atom_pixeltransfer.c \
	state_tracker/st_atom_rasterizer.c \
	state_tracker/st_atom_sampler.c \
	state_tracker/st_atom_scissor.c \
	state_tracker/st_atom_shader.c \
	state_tracker/st_atom_stipple.c \
	state_tracker/st_atom_texture.c \
	state_tracker/st_atom_viewport.c \
	state_tracker/st_cb_accum.c \
	state_tracker/st_cb_bitmap.c \
	state_tracker/st_cb_blit.c \
	state_tracker/st_cb_bufferobjects.c \
	state_tracker/st_cb_clear.c \
	state_tracker/st_cb_fbo.c \
	state_tracker/st_cb_get.c \
	state_tracker/st_cb_flush.c \
	state_tracker/st_cb_program.c \
	state_tracker/st_cb_queryobj.c \
	state_tracker/st_cb_readpixels.c \
	state_tracker/st_cb_strings.c \
	state_tracker/st_cb_texture.c \
	state_tracker/st_context.c \
	state_tracker/st_debug.c \
	state_tracker/st_draw.c \
	state_tracker/st_extensions.c \
	state_tracker/st_format.c \
	state_tracker/st_framebuffer.c \
	state_tracker/st_gen_mipmap.c \
	state_tracker/st_mesa_to_tgsi.c \
	state_tracker/st_program.c \
	state_tracker/st_texture.c

STATE_TRACKER_H_LINKS = \
	state_tracker/st_atom_constbuf.h \
	state_tracker/st_atom.h \
	state_tracker/st_atom_shader.h \
	state_tracker/st_cache.h \
	state_tracker/st_cb_accum.h \
	state_tracker/st_cb_bitmap.h \
	state_tracker/st_cb_blit.h \
	state_tracker/st_cb_bufferobjects.h \
	state_tracker/st_cb_clear.h \
	state_tracker/st_cb_fbo.h \
	state_tracker/st_cb_get.h \
	state_tracker/st_cb_flush.h \
	state_tracker/st_cb_program.h \
	state_tracker/st_cb_queryobj.h \
	state_tracker/st_cb_readpixels.h \
	state_tracker/st_cb_strings.h \
	state_tracker/st_cb_texture.h \
	state_tracker/st_context.h \
	state_tracker/st_debug.h \
	state_tracker/st_draw.h \
	state_tracker/st_extensions.h \
	state_tracker/st_format.h \
	state_tracker/st_gen_mipmap.h \
	state_tracker/st_mesa_to_tgsi.h \
	state_tracker/st_program.h \
	state_tracker/st_public.h \
	state_tracker/st_texture.h


# this isn't really needed, but informative
MISC_OMITTED = \
	glapi/glapi.c \
	shader/grammar/grammar.c \
	shader/grammar/grammar_crt.c \
	shader/slang/library/syn_to_c \
	shader/slang/library/syn_to_c.c \
	shader/slang/library/gc_to_bin.c \
	shader/atifragshader.c \
	shader/atifragshader.h \
	shader/nvfragparse.c \
	shader/nvfragparse.h \
	shader/nvprogram.c \
	shader/nvprogram.h \
	shader/nvvertparse.c \
	shader/nvvertparse.h \
	shader/prog_debug.c \
	shader/prog_debug.h \
	state_tracker/st_cb_drawpixels.c \
	state_tracker/st_cb_drawpixels.h \
	state_tracker/st_cb_feedback.c \
	state_tracker/st_cb_feedback.h \
	state_tracker/st_cb_rasterpos.c \
	state_tracker/st_cb_rasterpos.h \
	vbo/vbo_save_api.c \
	vbo/vbo_save.c \
	vbo/vbo_save_draw.c \
	vbo/vbo_save_loopback.c \
	vbo/vbo_save.h



C_SYMLINKS = \
	$(MAIN_C_LINKS) \
	$(MATH_C_LINKS) \
	$(GLAPI_C_LINKS) \
	$(SHADER_C_LINKS) \
	$(VBO_C_LINKS) \
	$(STATE_TRACKER_C_LINKS)

H_SYMLINKS = \
	$(MAIN_H_LINKS) \
	$(MATH_H_LINKS) \
	$(GLAPI_H_LINKS) \
	$(SHADER_H_LINKS) \
	$(STATE_TRACKER_H_LINKS) \
	$(VBO_H_LINKS)


SYMLINKS = $(C_SYMLINKS) $(H_SYMLINKS)


LOCAL_SOURCES = \
	main/pixel.c \
	state_tracker/st_cb_drawtex.c


SOURCES = \
	$(C_SYMLINKS) \
	$(LOCAL_SOURCES)


OBJECTS = $(SOURCES:.c=.o) \


