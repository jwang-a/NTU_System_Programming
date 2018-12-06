#Compile:
	make all

#Clean:
	make clean

#Execution:
	To test combination of the three programs:
	  ./bidding_system [number_of_hosts] [number_of_players]

	Host still seems a bit slow, please bear with it
        Written to be compatible with Sample progam

#Description:
	Nothing special, straight-forward approach for first three task.
	player_bonus features a simple reflex agent which
	  1.Would not pay over 3400 in any circumstances
          1.Takes the prize if a certain win is gauranteed under rule1
            (namely it has more money compared to the other three players)
          2.Decides how much to play between [money_owned] and [money_owned-5]
	    by chance

#Self_Examination:
	All seven tasks plus bonus finished.
          Task 1,4,5 tested on workstation with every possible combination
	  Task 2,3   checked by tracking process status while testing
	  Task 6     finished by successing the first five tasks
	  Task 7     checked by testing makefile
	Additional test for bonus done by modifying host and running mock plays
        between several versions of player_bonus
