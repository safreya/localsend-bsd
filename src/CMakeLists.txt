add_executable(localsend
	main.cpp
	global.cpp 
	self_info.cpp
	sha.cpp
	udp.cpp
	send.cpp
	recv.cpp
	selfcert.cpp
)
target_link_libraries(localsend ssl crypto pthread magic curl cpp-httplib)
install(TARGETS localsend DESTINATION bin)
