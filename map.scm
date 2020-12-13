(define (transform-pos l) (list (* 64 (+ 280 (car l))) (* 64 (+ 130 (cadr l)))))
(define (c-c-t x y) (mk-crate (list (* 64 (+ x 280)) (* 64 (+ y 130)))))

(c-c-t -18 0)
(c-c-t -19 -20)
(c-c-t -21 -49)
(c-c-t -4 -49)

(mk-spawner (transform-pos '(-60 -20)))

;(c-c-t -80 0)
(mk-crate-inner '() (list (* 64 200) (* 64 130)) 256)

(define (mk-hero n t) (mk-player (list
	(+ 6400 (quotient (* n 6400) t))
	6400
)))

(let ((y (* 150 64)))
(let loop ((x (* 50 64)))
	(if (< x (* 300 64)) (begin
		(mk-ground (list x y))
		(loop (+ x (* 16 64)))
	))
)
)
