cmake_minimum_required (VERSION 2.8)
project (leechcraft_lmp_ppl)
include (InitLCPlugin OPTIONAL)

set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include_directories (${LMP_INCLUDE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${LEECHCRAFT_INCLUDE_DIR}
	)

set (PPL_SRCS
	loghandler.cpp
	parser.cpp
	ppl.cpp
	tracksselectordialog.cpp
	)
set (PPL_FORMS
	tracksselectordialog.ui
	)
QtWrapUi (PPL_UIS_H ${PPL_FORMS})

CreateTrs ("lmp_ppl" "en;ru_RU" PPL_COMPILED_TRANSLATIONS)
CreateTrsUpTarget("lmp_ppl" "en;ru_RU" "${PPL_SRCS}" "${PPL_FORMS}" "")

add_library (leechcraft_lmp_ppl SHARED
	${PPL_COMPILED_TRANSLATIONS}
	${PPL_SRCS}
	${PPL_UIS_H}
	)
target_link_libraries (leechcraft_lmp_ppl
	${LEECHCRAFT_LIBRARIES}
	)

install (TARGETS leechcraft_lmp_ppl DESTINATION ${LC_PLUGINS_DEST})
#install (FILES lmppplsettings.xml DESTINATION ${LC_SETTINGS_DEST})
install (FILES ${PPL_COMPILED_TRANSLATIONS} DESTINATION ${LC_TRANSLATIONS_DEST})

FindQtLibs (leechcraft_lmp_ppl Widgets)

option (ENABLE_LMP_PPL_TESTS "Build tests for LMP PPL" ON)

if (ENABLE_LMP_PPL_TESTS)
	function (AddPPLTest _execName _cppFile _testName)
		set (_fullExecName lc_lmp_ppl_${_execName}_test)
		add_executable (${_fullExecName} WIN32 ${_cppFile})
		target_link_libraries (${_fullExecName} ${LEECHCRAFT_LIBRARIES})
		add_test (${_testName} ${_fullExecName})
		FindQtLibs (${_fullExecName} Concurrent Test)
		add_dependencies (${_fullExecName} leechcraft_lmp_ppl)
	endfunction ()

	AddPPLTest (parser tests/parsertest.cpp LMPPPLParserTest)
endif ()
