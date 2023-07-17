cmake_minimum_required(VERSION 3.26)

project(github_issue_linker)

add_executable(github_issue_linker src/main.c)

target_link_libraries(github_issue_linker PUBLIC discord curl dotenv pcre)