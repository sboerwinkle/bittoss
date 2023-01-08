(define (mk-player pos team)
	(set-tick (set-draw (set-who-moves (set-pushed (set-push
		(create
			'()
			(list 512 512 512)
			(+ T_OBSTACLE (* team TEAM_BIT))
			(+ T_OBSTACLE T_TERRAIN)
			pos
			6
			0
		)
	player-push) player-pushed) player-whomoves) player-draw) player-tick)
)
(display "wow neat\n")
