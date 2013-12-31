file(GLOB MANPAGES *.[0-9])
foreach(MANPAGE ${MANPAGES})
  if(${MANPAGE} MATCHES ".([0-9])$")
    set(SECTION_NUMBER ${CMAKE_MATCH_1})
    install(FILES ${MANPAGE} DESTINATION ${MAN_SUBDIR}/man${SECTION_NUMBER})
  endif()
endforeach()