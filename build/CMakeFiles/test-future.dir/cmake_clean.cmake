file(REMOVE_RECURSE
  "../msg_gen"
  "../msg_gen"
  "../src/neural-cam-ros/msg"
)

# Per-language clean rules from dependency scanning.
foreach(lang)
  include(CMakeFiles/test-future.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
