%VULKAN_SDK%/bin/glslc.exe shaders/im3d_points.vert -o shaders/im3d_points.vert.spv
%VULKAN_SDK%/bin/glslc.exe shaders/im3d_points.frag -o shaders/im3d_points.frag.spv

%VULKAN_SDK%/bin/glslc.exe shaders/im3d_lines.vert -o shaders/im3d_lines.vert.spv
%VULKAN_SDK%/bin/glslc.exe shaders/im3d_lines.frag -o shaders/im3d_lines.frag.spv

%VULKAN_SDK%/bin/glslc.exe shaders/im3d_tris.vert -o shaders/im3d_tris.vert.spv
%VULKAN_SDK%/bin/glslc.exe shaders/im3d_tris.frag -o shaders/im3d_tris.frag.spv

bin2c /custvar "im3d_tris_vert_spv_bin" /infile "shaders/im3d_tris.vert.spv" /outfile "shaders/im3d_tris.vert.spv.h"
bin2c /custvar "im3d_tris_frag_spv_bin" /infile "shaders/im3d_tris.frag.spv" /outfile "shaders/im3d_tris.frag.spv.h"
bin2c /custvar "im3d_points_vert_spv_bin" /infile "shaders/im3d_points.vert.spv" /outfile "shaders/im3d_points.vert.spv.h"
bin2c /custvar "im3d_points_frag_spv_bin" /infile "shaders/im3d_points.frag.spv" /outfile "shaders/im3d_points.frag.spv.h"
bin2c /custvar "im3d_lines_vert_spv_bin" /infile "shaders/im3d_lines.vert.spv" /outfile "shaders/im3d_lines.vert.spv.h"
bin2c /custvar "im3d_lines_frag_spv_bin" /infile "shaders/im3d_lines.frag.spv" /outfile "shaders/im3d_lines.frag.spv.h"