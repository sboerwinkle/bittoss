(define (mk-flag-spawner pos team)
	(let
		((s (create
			'()
			(list 1 1 1)
			T_WEIGHTLESS
			0
			pos
			2
			0
		)))
		(set-slider (get-state s) 1 team)
		(set-tick s flag-spawner-tick)
		(set-draw s no-draw)
	)
)

(define (mk-flag owner team)
	(set-tick
		(set-pushed
			(set-draw
				(set-who-moves
					(create
						owner
						(list 350 350 350)
						(+ T_FLAG T_OBSTACLE (* team TEAM_BIT))
						(+ T_OBSTACLE T_TERRAIN)
						(list 0 0 0)
						2
						0
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
