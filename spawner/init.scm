(define (seed-pushed me him axis dir dx dv)
	(if (= 1 (get-slider (get-state me) 0)) R_DROP R_DIE)
)

(define (become-crate me)
	(mk-crate-inner me '(0 0 0) (car (get-radius me)))
	(kill me)
)

(define (mk-crate-seed owner pos r)
	(let ((x (create owner (list r r r) T_OBSTACLE (+ T_OBSTACLE T_TERRAIN) pos 1 0)))
		(set-tick x become-crate); Note this isn't tick-on-held, hence the magic!
		(set-pushed x seed-pushed) ; Ugh this will have to be converted if we intend to use it
		(set-draw x crate-draw)
		(set-who-moves x move-me) ; Ugh this will have to be converted if we intend to use it
	)
)


(define (spawner-tick me)
	(if
		(= 0 (count-holdees me (lambda (x)
			(set-slider (get-state x) 0
				(if (move-to x (list 0 0 (* -32 64)) 32)
					1
					0
				)
			)
			#t
		)))
		(let* ((state (get-state me)) (t (get-slider state 0)))
			(if (> t 60)
				(begin
					(set-slider state 0 0)
					(pickup me (mk-crate-seed me '(0 0 0) (- (car (get-radius me)) 64)))
				)
				(set-slider state 0 (+ 1 t))
			)
		)
	)
)
(define spawner-draw (base-draw 0.5 0.5 0.0))


(define (mk-spawner-inner owner pos r)
	(set-tick
		(set-draw
			(set-who-moves
				(create owner (list r r r) (+ T_OBSTACLE T_HEAVY) (+ T_OBSTACLE T_TERRAIN) pos 1 0)
				dont-move-terrain ; Ugh this will have to be converted if we intend to use it
			)
			spawner-draw
		)
		spawner-tick
	)
)
(define (mk-spawner pos) (mk-spawner-inner '() pos (+ 64 512)))
