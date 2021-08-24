(define (transform-pos l) (list (* 64 (+ 280 (car l))) 0 (* 64 (+ 130 (cadr l)))))
(define (c-c-t x y) (mk-crate (list (* 64 (+ x 280)) 0 (* 64 (+ y 130)))))

(c-c-t -18 0)
(c-c-t -19 -20)
(c-c-t -21 -49)
(c-c-t -4 -49)

(mk-spawner (transform-pos '(-60 -20)))

;(c-c-t -80 0)
(mk-crate-inner '() (list (* 64 200) 0 (* 64 130)) 256)

(define (mk-hero n t) (mk-player (list
	(+ 6400 (quotient (* n 6400) t))
	0
	6400
)))

;(define (loop x)
;(let ((y (* 150 64)))
;	(if (< x (* 300 64)) (begin
;		(mk-ground (list x 0 y))
;		(loop (+ x (* 16 64)))
;	))
;)
;)
;(loop (* 50 64))

(mk-ground-inner (list (* 175 64) 0 (* 150 64)) (list (* 130 64) (* 130 64) 512))
