(define (stackem-who-moves a b axis dir)
	(if (typ? b T_TERRAIN)
		MOVE_ME
		(if (= axis 2)
			(if (and (= -1 dir) (typ? b T_HEAVY)) MOVE_ME MOVE_HIM)
			(if (= 0 (& TEAM_MASK (get-type a) (get-type b))) MOVE_HIM MOVE_ME)
		)
	)
)
(define (stackem-pushed me him axis dir dx dv)
	(if (and (= dir -1) (= axis 2))
		(let ((state (get-state me)) (vel (get-vel me him)))
			(set-slider state 0 (bound (car vel) 4))
			(set-slider state 1 (bound (cadr vel) 4))
		)
	)
	R_PASS
)
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
					stackem-who-moves
				)
				stackem-draw
			)
			stackem-pushed
		)
		stackem-tick
	)
)
