(define (mk-flag pos team)
	(set-tick
		(set-pushed
			(set-draw
				(set-who-moves
					(create
						'()
						(list 350 350 350)
						(+ T_FLAG T_OBSTACLE (* team TEAM_BIT))
						(+ T_OBSTACLE T_TERRAIN)
						pos
						2
					)
					flag-whomoves
				)
				player-draw
			)
			stackem-pushed
		)
		stackem-tick
	)
)
