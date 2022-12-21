;(define (transform-pos l) (list (* 64 (+ 280 (car l))) 0 (* 64 (+ 130 (cadr l)))))
;(define (c-c-t x y) (mk-crate (list (* 64 (+ x 280)) 0 (* 64 (+ y 130)))))
;
;(c-c-t -18 0)
;(c-c-t -19 -20)
;(c-c-t -21 -49)
;(c-c-t -4 -49)
;
;(mk-spawner (transform-pos '(-60 -20)))
;
;;(c-c-t -80 0)
;(mk-crate-inner '() (list (* 64 200) 0 (* 64 130)) 256)

; Even players on one team; odds on another
(define (pick-team n) (+ 1 (modulo n 2)))
; FFA
;(define (pick-team n) 0)

(define (mk-hero n t)
	(let ((denom (if (> t 1) (- t 1) 1)))
		(mk-player
			(list
				(+ (* -2 3200) (quotient (* n 3200 4) denom))
				0
				-16000
			)
			(pick-team n)
		)
	)
)

;(define (loop x)
;(let ((y (* 150 64)))
;	(if (< x (* 300 64)) (begin
;		(mk-ground (list x 0 y))
;		(loop (+ x (* 16 64)))
;	))
;)
;)
;(loop (* 50 64))

(define clr-white (base-draw 1.0 1.0 1.0))
(define clr-blue (base-draw 0.8 0.8 1.0))

(define clr-mag-1 (base-draw 1.0 0.0 1.0))
(define clr-mag-2 (base-draw 0.7 0.0 0.7))
(define clr-mag-3 (base-draw 0.4 0.0 0.4))

(let*
	(
		(width 3200)
		(dims (list width width 512))
		(f (lambda (x y clr) (mk-ground-inner (list (* width x 2) (* width y 2) 0) dims clr)))
	)
	(f 0 0 clr-white)
	(f 1 1 clr-white)
	(f -1 1 clr-white)
	(f 1 -1 clr-white)
	(f -1 -1 clr-white)
	(f 0 -1 clr-blue)
	(f 0 1 clr-blue)
	(f 1 0 clr-blue)
	(f -1 0 clr-blue)
	(mk-flag (list  width 0 -1024) 2)
	(mk-flag (list (* -1 width) 0 -1024) 1)
)
