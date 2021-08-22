; TODO: Not working in 3D yet

; defines a simple helper, and then a test object that uses it.

(define (move-to me pos spd)
	(let ((h (get-holder me)))
		(let ((dx (map - pos (get-pos h me))))
			(accel me (map
				; "bound" defned in player/init.scm
				(lambda (dx v) (- (bound dx spd) v))
				dx
				(get-vel h me)
			))
			(and (= (car dx) 0) (= (cadr dx) 0) (= (cadr (cdr dx)) 0)) ; TODO Just use eq?, maybe?
		)
	)
)



(define (move-to-tester-tick-held me)
	(let* ((state (get-state me)) (s (lambda (x) (get-slider state x))))
		(set-slider state 2
			(if (move-to me (list (s 0) (s 1)) 32)
				1
				0
			)
		)
	)
)

(define move-to-tester-draw (base-draw 0.1 0.5 0.1))
(define brick-draw (base-draw 0.5 0.1 0.1))
;TODO move common stuff to common place
(define (move-me a b axis dir) MOVE_ME)
(define (dont-move-terrain a b axis dir) (if (typ? b T_TERRAIN) MOVE_ME MOVE_HIM))
(define (pushed-pass me him axis dir dx dv) R_PASS)
;(define (kill-me me) (kill me))

(define (mk-move-to-tester owner pos w h x y)
	(let ((new (create owner w h (+ T_GROW T_OBSTACLE) (+ T_OBSTACLE T_TERRAIN) pos 3)))
		(let ((s (get-state new)))
			(set-slider s 0 x)
			(set-slider s 1 y)
		)
		(set-tick-held new move-to-tester-tick-held)
		(set-tick new kill)
		(set-pushed new pushed-pass)
		(set-draw new move-to-tester-draw)
		(set-who-moves new move-me)
	)
)

(define (mk-seed pos)
	(set-tick
		(set-who-moves
			(set-draw
				(create '() 256 256 T_OBSTACLE (+ T_OBSTACLE T_TERRAIN) pos 1)
				move-to-tester-draw
			)
			move-me
		)
		seed-tick
	)
)

(define (mk-brick owner w h pos)
	(set-who-moves
		(set-draw
			(create owner w h (+ T_OBSTACLE T_HEAVY) (+ T_OBSTACLE T_TERRAIN) pos 0)
			brick-draw
		)
		dont-move-terrain
	)
)

(define (move-to-test owner pos)
	;(pickup owner (mk-move-to-tester owner (map - pos (get-abs-pos owner '(0 0)))))
	(mk-seed pos)
)

(define (growth? x) (typ? x T_GROW))
(define (grown? x) (and (growth? x) (= 1 (get-slider (get-state x) 2))))

(define (seed-tick me)
	(let ((state (get-state me))) (let
		(
			(phase (get-slider state 0))
			(wave (lambda (x) (begin
					(set-slider state 0 (* 2 x))
					(let ((f (lambda (a b) (pickup me (mk-move-to-tester me '(0 0) x x (* x a) (* x b))))))
						(f 1 1)
						(f 1 -1)
						(f -1 1)
						(f -1 -1)
					)
			)))
		)
		(if (= phase 0)
			;(if (move-to me '(0 4096) 32) (wave 256) '())
			(wave (car (get-radius me)))

			; Grow in waves
			(let ((growths (count-holdees me growth?)))
				(if (< growths 4)
					(kill me)
					(if (= growths (count-holdees me grown?))
						(begin
							(count-holdees me (lambda (x) (if (growth? x) (kill x) '())))
							(if (< phase 4096)
								(wave phase)
								; Die + add brick to parent
								(begin
									(kill me)
									(pickup (get-holder) (mk-brick me phase phase '(0 0)))
								)
							)
						)
						'()
					)
				)
			)
		)
	))
)
