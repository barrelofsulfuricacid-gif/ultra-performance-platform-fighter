if(NOT DEFINED PF_M4_TECHNIQUE_REGISTRY)
    message(FATAL_ERROR "PF_M4_TECHNIQUE_REGISTRY is required")
endif()

if(NOT EXISTS "${PF_M4_TECHNIQUE_REGISTRY}")
    message(FATAL_ERROR "M4 technique registry is missing")
endif()

file(READ "${PF_M4_TECHNIQUE_REGISTRY}" registry)

foreach(
    required_marker
    "**Registry schema:** 1"
    "revision 2048934"
    "**M4 acceptance:** Blocked")
    string(FIND "${registry}" "${required_marker}" marker_offset)
    if(marker_offset EQUAL -1)
        message(
            FATAL_ERROR
            "M4 technique registry is missing marker: ${required_marker}")
    endif()
endforeach()

string(
    REGEX MATCHALL
    "\\|[ ]+[0-9]+[ ]+\\|[ ]+[^|]+[ ]+\\| (planned|primitive-ready|playable|verified) \\| M4\\.4 \\|"
    registry_rows
    "${registry}")
list(LENGTH registry_rows row_count)
if(NOT row_count EQUAL 61)
    message(
        FATAL_ERROR
        "M4 technique registry expected 61 valid rows, found ${row_count}")
endif()

foreach(row_id RANGE 1 61)
    string(
        REGEX MATCH
        "\n\\|[ ]+${row_id}[ ]+\\|"
        row_match
        "${registry}")
    if(row_match STREQUAL "")
        message(
            FATAL_ERROR
            "M4 technique registry is missing row ${row_id}")
    endif()
endforeach()

foreach(status planned primitive-ready playable verified)
    string(
        REGEX MATCHALL
        "\\| ${status} \\| M4\\.4 \\|"
        "${status}_rows"
        "${registry}")
    list(LENGTH "${status}_rows" "${status}_count")
endforeach()

if(NOT planned_count EQUAL 7 OR
   NOT primitive-ready_count EQUAL 3 OR
   NOT playable_count EQUAL 50 OR
   NOT verified_count EQUAL 1)
    message(
        FATAL_ERROR
        "M4 technique status counts changed: planned=${planned_count} "
        "primitive_ready=${primitive-ready_count} playable=${playable_count} "
        "verified=${verified_count}")
endif()

message(
    STATUS
    "m4-technique-registry=pass rows=61 verified=1 playable=50 "
    "primitive_ready=3 planned=7 acceptance=blocked")
