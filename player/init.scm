;(define player-draw (base-draw 0.3 1.0 0.3))
(define (player-draw me)
	(draw
		me
		(if (typ? me TEAM_BIT) 1.0 0.5)
		(if (typ? me (* 2 TEAM_BIT)) 1.0 0.5)
		(if (typ? me (* 4 TEAM_BIT)) 1.0 0.5)
	)
)

(define (player-tick me)
	(let ((state (get-state me)))
	(let ((axis (get-axis me)) (grounded (> (get-slider state 2) 0)) (slider (list (get-slider state 0) (get-slider state 1))))
		(let ((dx (map (lambda (a s) (bound (- (* 4 a) s) 10)) axis slider)))
			(set-slider state 0 (+ (car slider) (car dx)))
			(set-slider state 1 (+ (cadr slider) (cadr dx)))
			(let ((d (if grounded 1 2)))
				(accel me (list (quotient (car dx) d) (quotient (cadr dx) d) 0))
			)
		)
		(if (and grounded (get-button me))
			(accel me (list 0 0 -192))
		)
		(set-slider state 2 0)
		;(if (get-button me) (kill me) '())
	)
	(let ((charge (get-slider state 3)) (cooldown (get-slider state 4)))
		(if (and (get-trigger me 1) (>= charge 180) (>= cooldown 10))
			(begin
				(set-slider state 3 0)
				(set-slider state 4 0)
				(mk-platform
					me
					; 0.0406 = 1.3 / axisMaxis   (axisMaxis == 32)
					(map (lambda (a b) (truncate (* a (+ 512 b) 0.0406))) (get-look me) platform-size)
					(let ((tag (get-slider state 5)))
						(set-slider state 5 (- 1 tag))
						(if (> tag 0) clr-white clr-blue)
					)
				)
			)
			(if (and (get-trigger me 0) (>= charge 60) (>= cooldown 10))
				(begin
					(set-slider state 3 (- charge 60))
					(set-slider state 4 0)
					(let ((look (get-look me)))
						(accel (mk-stackem me look) (map (lambda (x) (* x 7)) look))
					)
				)
				(begin
					(if (< charge 180) (set-slider state 3 (+ 1 charge)))
					(if (< cooldown 10) (set-slider state 4 (+ 1 cooldown)))
				)
			)
		)
	)
	)
)

(define (mk-player pos team)
	(set-tick (set-draw (set-who-moves (set-pushed
		(create
			'()
			(list 512 512 512)
			(+ T_OBSTACLE (* team TEAM_BIT))
			(+ T_OBSTACLE T_TERRAIN)
			pos
			6
			0
		)
	player-pushed) player-whomoves) player-draw) player-tick)
)
(display "wow neat\n")
