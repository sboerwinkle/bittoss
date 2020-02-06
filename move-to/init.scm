; defines a simple helper, and then a test object that uses it.

(define (move-to me pos spd)
	(let ((h (get-holder me)))
		(let ((dx (map - pos (get-pos h me))))
			(apply accel (cons me (map
				; "bound" defned in player/init.scm
				(lambda (dx v) (- (bound dx spd) v))
				dx
				(get-vel h me)
			)))
			(and (= (car dx) 0) (= (cadr dx) 0))
		)
	)
)



(define (move-to-tester-tick-held me)
	(move-to me '(0 1024) 32)
)

(define move-to-tester-draw (base-draw 0.1 0.5 0.1))
;TODO move common stuff to common place
(define (move-me a b axis dir) MOVE_ME)
(define (pushed-pass me him axis dir dx dv) R_PASS)

(define (mk-move-to-tester owner pos)
	(set-tick-held
		(set-pushed
			(set-draw
				(set-who-moves
					(create owner 256 256 T_OBSTACLE (+ T_OBSTACLE T_TERRAIN) pos 0)
					'move-me
				)
				'move-to-tester-draw
			)
			'pushed-pass
		)
		'move-to-tester-tick-held
	)
)

(define (move-to-test owner pos)
	(pickup owner (mk-move-to-tester owner (map - pos (get-abs-pos owner '(0 0)))))
)
