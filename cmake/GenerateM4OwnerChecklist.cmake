function(json_escape input output)
    set(value "${input}")
    string(REPLACE "\\" "\\\\" value "${value}")
    string(REPLACE "\"" "\\\"" value "${value}")
    string(REPLACE "\r" "" value "${value}")
    string(REPLACE "\n" "\\n" value "${value}")
    string(REPLACE "\t" "\\t" value "${value}")
    set("${output}" "${value}" PARENT_SCOPE)
endfunction()

function(pf_generate_owner_checklist registry_path output_path)
    if(NOT EXISTS "${registry_path}")
        message(FATAL_ERROR "M4 owner checklist registry is missing")
    endif()

    file(READ "${registry_path}" registry)
    get_filename_component(output_directory "${output_path}" DIRECTORY)
    file(MAKE_DIRECTORY "${output_directory}")

    string(CONCAT output
        "(function (root) {\n"
        "  \"use strict\";\n"
        "  root.PF_M4_OWNER_CHECKLIST = {\n"
        "    schema: 1,\n"
        "    sourceRevision: \"2048934\",\n"
        "    techniques: [\n")

    foreach(row_id RANGE 1 61)
        string(
            REGEX MATCH
            "\n\\|[ ]*${row_id}[ ]*\\|[^\r\n]*"
            row
            "${registry}")
        string(REGEX REPLACE "^\n" "" row "${row}")

        if(NOT row MATCHES
           "^\\|[ ]*([0-9]+)[ ]*\\|[ ]*([^|]+)[ ]*\\|[ ]*([^|]+)[ ]*\\|[ ]*([^|]+)[ ]*\\|[ ]*([^|]+)[ ]*\\|[ ]*([^|]+)[ ]*\\|[ ]*([^|]+)[ ]*\\|$")
            message(
                FATAL_ERROR
                "M4 owner checklist could not parse registry row ${row_id}")
        endif()

        set(technique "${CMAKE_MATCH_2}")
        set(status "${CMAKE_MATCH_3}")
        set(recipe "${CMAKE_MATCH_7}")
        string(STRIP "${technique}" technique)
        string(STRIP "${status}" status)
        string(STRIP "${recipe}" recipe)
        json_escape("${technique}" technique_json)
        json_escape("${status}" status_json)
        json_escape("${recipe}" recipe_json)

        if(row_id EQUAL 61)
            set(comma "")
        else()
            set(comma ",")
        endif()
        string(
            APPEND
            output
            "      { id: ${row_id}, name: \"${technique_json}\", "
            "registryStatus: \"${status_json}\", "
            "recipe: \"${recipe_json}\" }${comma}\n")
    endforeach()

    string(
        APPEND
        output
        "    ]\n"
        "  };\n"
        "})(typeof globalThis !== \"undefined\" ? globalThis : this);\n")
    file(WRITE "${output_path}" "${output}")
endfunction()
