build_lib(
  LIBNAME futurenet-ntn
  SOURCE_FILES
    helper/ntn-helper.cc
    model/circular-orbit-mobility-model.cc
    model/ntn-association-policy.cc
    model/ntn-attribute-checkers.cc
    model/ntn-ground-terminal-mobility-model.cc
    model/ntn-ip-link-controller.cc
    model/ntn-isl-model.cc
    model/ntn-link-manager.cc
    model/ntn-propagation-delay-model.cc
    model/ntn-trace-writer.cc
    model/ntn-visibility-model.cc
    model/ntn-walker-constellation.cc
  HEADER_FILES
    helper/ntn-helper.h
    model/circular-orbit-mobility-model.h
    model/ntn-association-policy.h
    model/ntn-attribute-checkers.h
    model/ntn-common-types.h
    model/ntn-constants.h
    model/ntn-ground-terminal-mobility-model.h
    model/ntn-ip-link-controller.h
    model/ntn-isl-model.h
    model/ntn-link-manager.h
    model/ntn-propagation-delay-model.h
    model/ntn-trace-writer.h
    model/ntn-visibility-model.h
    model/ntn-walker-constellation.h
  LIBRARIES_TO_LINK
    ${libcore}
    ${libmobility}
    ${libnetwork}
    ${libinternet}
    ${libpoint-to-point}
  TEST_SOURCES
    test/futurenet-ntn-test-suite.cc
)

# Record the ns-3 version and this module's Git commit for run-info files (Common Guidelines
# section 8.3). Both are read when ns-3 is configured; re-run "./ns3 configure" after committing
# to refresh the commit.
file(STRINGS ${PROJECT_SOURCE_DIR}/VERSION ntn_ns3_version LIMIT_COUNT 1)
execute_process(
  COMMAND git -c safe.directory=* describe --always --dirty
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE ntn_git_commit
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT ntn_git_commit)
  set(ntn_git_commit "unknown")
endif()
set_source_files_properties(
  helper/ntn-helper.cc
  PROPERTIES COMPILE_DEFINITIONS
  "FUTURENET_NTN_NS3_VERSION=\"${ntn_ns3_version}\";FUTURENET_NTN_GIT_COMMIT=\"${ntn_git_commit}\""
)
