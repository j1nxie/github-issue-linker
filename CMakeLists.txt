cmake_minimum_required(VERSION 3.26)

project(github_issue_linker)

add_executable(app src/main.c)

target_link_libraries(app PUBLIC discord curl dotenv pcre)