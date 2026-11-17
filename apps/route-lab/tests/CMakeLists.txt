set(route_fixture "${PROJECT_SOURCE_DIR}/tests/fixtures/route.csv")
add_test(NAME Lab.CLI COMMAND solar-race --route "${route_fixture}" --trials 3 --seed 12)
set_tests_properties(Lab.CLI PROPERTIES PASS_REGULAR_EXPRESSION "3,reserve-first,")
add_test(NAME Lab.EmbeddedRoute COMMAND solar-race --trials 2 --seed 42)
set_tests_properties(Lab.EmbeddedRoute PROPERTIES PASS_REGULAR_EXPRESSION "2,reserve-first,")
add_test(NAME Lab.CruiserAtlas COMMAND solar-race --atlas-report --trials 2 --trace-trial 2 --seed 42 --model mapped)
set_tests_properties(Lab.CruiserAtlas PROPERTIES PASS_REGULAR_EXPRESSION "2025-08-23T22:30:00Z")
add_test(NAME Lab.SR7Cycle COMMAND solar-race --route "${route_fixture}"
    --vehicle sr7 --model cycle --trials 2 --seed 42)
add_test(NAME Lab.SR8Mapped COMMAND solar-race --route "${route_fixture}"
    --vehicle sr8 --model mapped --trials 2 --seed 42)
add_test(NAME Lab.InvalidPack COMMAND solar-race --pack-kwh 0)
set_tests_properties(Lab.InvalidPack PROPERTIES WILL_FAIL TRUE)

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_FOUND AND Python3_VERSION VERSION_GREATER_EQUAL 3.9)
    add_test(NAME Lab.Analysis
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s tests -p "test_*.py"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
    add_test(NAME Lab.ExperimentMatrix
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/smoke_matrix.py"
                "$<TARGET_FILE:solar-race>" "${route_fixture}")
endif()

find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
    add_test(NAME Lab.Replay COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_replay.mjs")
    add_test(NAME Lab.AtlasFrontend COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_cruiser_atlas.mjs")
endif()
