
dsdiso = static_library(
  'dsdiso',
  'libdstdec/ac_data.cpp',
  'libdstdec/coded_table.cpp',
  'libdstdec/dst_decoder.cpp',
  'libdstdec/dst_decoder_mt.cpp',
  'libdstdec/frame_reader.cpp',
  'libdstdec/str_data.cpp',
  'libsacd/sacd_disc.cpp',
  'libsacd/scarletbook.cpp',
  include_directories: inc,
)

dsdiso_dep = declare_dependency(
  link_with: dsdiso,
)
