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