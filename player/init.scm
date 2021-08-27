(define (player-who-moves a b axis dir) (if (typ? b T_TERRAIN) MOVE_ME MOVE_BOTH))
;(define player-draw (base-draw 0.3 1.0 0.3))
(define (player-draw me)
	(draw
		me
		(if (typ? me TEAM_BIT) 1.0 0.5)
		(if (typ? me (* 2 TEAM_BIT)) 1.0 0.5)
		(if (typ? me (* 4 TEAM_BIT)) 1.0 0.5)
	)
)
;(define (player-tick me) (accel me (map (lambda (x) (* 10 x)) (get-axis me))))

(define (player-pushed me him axis dir dx dv)
	(let ((state (get-state me)))
	(if (= axis 2)
		; This used to be a case where bouncing off something laterally in the air would re-center your inertia;
		; However, it became tricky to make sure it was correct in all situations.
		;(if (< 0 (* dir (get-slider state axis)))
			;(set-slider state axis 0)
		;)
		(if (< dir 0)
			(let ((vel (get-vel him me)))
				(set-slider state 0 (bound (car vel) 128))
				(set-slider state 1 (bound (cadr vel) 128))
				(set-slider state 2 1)
			)
		)
	)
	)
	R_PASS
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
		(if (and (get-trigger me) (>= charge 60) (>= cooldown 10))
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

(define (mk-player pos team)
	(set-tick (set-draw (set-who-moves (set-pushed
		(create
			'()
			(list 512 512 512)
			(+ T_OBSTACLE (* team TEAM_BIT))
			(+ T_OBSTACLE T_TERRAIN)
			pos
			5
		)
	player-pushed) player-who-moves) player-draw) player-tick)
)
(display "wow neat")
