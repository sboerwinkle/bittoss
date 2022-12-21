(define (crate-who-moves a b axis dir)
	(if
		(and
			(= axis 2)
			(= 1 dir)
			(not (typ? b T_TERRAIN)
		)
		MOVE_HIM
		MOVE_ME
	)
)
(define (crate-pushed me him axis dir dx dv)
	(let ((state (get-state me)) (slider (+ 1 dir axis)))
		(if (and (< axis 2) (= 0 (get-slider state slider)))
			(set-slider state slider 2)
		)
	)
	R_PASS
)
(define (crate-tick me)
	(let ((state (get-state me)) (radius (get-radius me)))
	(let ((w (car radius)) (h (cadr radius)) (r (/ (car radius) 2)))
	(let
		((helper (lambda (slider m1 m2)
			(if (= 2 (get-slider state slider))
				(begin
					(set-slider state slider 1)
					(pickup me (mk-crate-inner me (list (* m1 (+ w r r)) (* m2 (+ w r r)) (- 0 h r)) r))
				)
			)
		)))
		(helper 0 -1  0)
		(helper 1  0 -1)
		(helper 2  1  0)
		(helper 3  0  1)
	)
	)
	)
)
(define crate-draw (base-draw 0.7 0.7 0.0))


(define (mk-crate-inner owner pos r)
	(set-tick
		(set-pushed
			(set-draw
				(set-who-moves
					(create owner (list r r r) (+ T_OBSTACLE T_HEAVY) (+ T_OBSTACLE T_TERRAIN) pos 4 0)
					crate-who-moves
				)
				crate-draw
			)
			crate-pushed
		)
		crate-tick
	)
)
(define (mk-crate pos) (mk-crate-inner '() pos 512))
