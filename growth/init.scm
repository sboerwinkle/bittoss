(define (move-me a b axis dir) MOVE_ME)
(define (die-on-pushed me him axis dir dx dv) R_DIE)
(define (growth-crush me)
	(let ((state (get-state me)))
		; For my recorded parent and slider, report failure
		; TODO can we just get whoever's holding me?
		(set-slider (get-state (get-ent state 0)) (get-slider state 0) -1)
	)
)
; TODO onFumble should crush
(define (growth-tick me)
	(let ((state (get-state me)))
	(let
		(
			(ax (if (> (get-slider state 1) 0) cadr car))
			(dest (get-slider state 2))
			(him (get-ent state 0))
		)
		(if ((if (> dest 0) >= <=) (ax (get-pos me him)) dest)
			(begin
				(set-crushed me '())
				(set-slider (get-state him) (get-slider state 0) 1)
				;TODO set his size / pos - and move his kids?
				(crush me)
			)
		)
	)
	)
)

;TODO among other things this should take take a draw-sym and who-moves-sym.
; Also don't forget dest (get-slider state 2) is inverted, since we use (get-pos me him)

(define (mk-growth args...)
	(set-tick
		(set-pushed
			(set-draw
				(set-who-moves
					(create owner r r (+ T_OBSTACLE T_HEAVY) (+ T_OBSTACLE T_TERRAIN) pos 2)
					'crate-who-moves
				)
				'crate-draw
			)
			'crate-pushed
		)
		'crate-tick
	)
)
