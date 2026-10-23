build_lib_example(
  NAME ntn-example-basic
  SOURCE_FILES ntn-example-basic.cc
  LIBRARIES_TO_LINK ${libfuturenet-ntn}
)

build_lib_example(
  NAME ntn-example-advanced
  SOURCE_FILES ntn-example-advanced.cc
  LIBRARIES_TO_LINK
    ${libfuturenet-ntn}
    ${libapplications}
    ${libflow-monitor}
)

build_lib_example(
  NAME ntn-validation
  SOURCE_FILES ntn-validation.cc
  LIBRARIES_TO_LINK ${libfuturenet-ntn}
)

build_lib_example(
  NAME ntn-example-constellation
  SOURCE_FILES ntn-example-constellation.cc
  LIBRARIES_TO_LINK
    ${libfuturenet-ntn}
    ${libapplications}
    ${libflow-monitor}
)
