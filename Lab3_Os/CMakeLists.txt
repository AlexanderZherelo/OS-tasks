    cmake_minimum_required(VERSION 3.8)

    project(Lab3_Markers LANGUAGES CXX)

    set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/Cmake_Lab3_os.cpp)



    # target для C++11
    add_executable(lab_markers_cpp11 ${SRC})
    set_target_properties(lab_markers_cpp11 PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    # target для C++17
    add_executable(lab_markers_cpp17 ${SRC})
    set_target_properties(lab_markers_cpp17 PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
