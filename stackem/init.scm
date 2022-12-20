(define (stackem-tick me)
	(let ((state (get-state me)))
		(accel me (list (get-slider state 0) (get-slider state 1) 0))
		(set-slider state 0 0)
		(set-slider state 1 0)
	)
)
(define (stackem-draw me)
	(draw
		me
		(if (typ? me TEAM_BIT) 0.5 0.3)
		(if (typ? me (* 2 TEAM_BIT)) 0.5 0.3)
		(if (typ? me (* 4 TEAM_BIT)) 0.5 0.3)
	)
)

; Offset is necessary so I don't spawn in the exact middle, which can lead to tears
(define (mk-stackem owner offset)
	(set-tick
		(set-pushed
			(set-draw
				(set-who-moves
					; radius = 12*32 = 384
					(create
						owner
						(list 450 450 450)
						(+ T_OBSTACLE T_HEAVY (& TEAM_MASK (get-type owner)))
						(+ T_OBSTACLE T_TERRAIN)
						offset
						2
					)
					stackem-whomoves
				)
				stackem-draw
			)
			stackem-pushed
		)
		stackem-tick
	)
)
