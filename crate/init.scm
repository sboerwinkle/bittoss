(define (crate-who-moves a b axis dir) (if (or (not (typ? b T_HEAVY)) (and axis (= 1 dir))) MOVE_HIM MOVE_ME))
(define (crate-pushed me him axis dir dx dv)
	(let ((state (get-state me)) (slider (/ (+ 1 dir) 2)))
		(if (and (not axis) (= 0 (get-slider state slider)))
			(set-slider state slider 2)
		)
	)
	R_PASS
)
(define (crate-tick me)
	(let ((state (get-state me)) (radius (get-radius me)))
	(let ((w (car radius)) (h (cadr radius)) (r (/ (car radius) 2)))
	(let
		((helper (lambda (slider dir)
			(if (= 2 (get-slider state slider))
				(begin
					(set-slider state slider 1)
					(pickup me (mk-crate-inner (get-abs-pos me (list (* dir (+ w r r)) (- 0 h r))) r))
				)
			)
		)))
		(helper 0 -1)
		(helper 1 1)
	)
	)
	)
)
(define crate-draw (base-draw 0.7 0.7 0.0))


(define (mk-crate-inner pos r)
	(set-tick
		(set-pushed
			(set-draw
				(set-who-moves
					(create r r (+ T_OBSTACLE T_HEAVY) (+ T_OBSTACLE T_TERRAIN) pos 2)
					'crate-who-moves
				)
				'crate-draw
			)
			'crate-pushed
		)
		'crate-tick
	)
)
(define (mk-crate pos) (mk-crate-inner pos 512))
