(define MOVE_ME 1)
(define MOVE_HIM 2)
(define MOVE_BOTH 7)
(define MOVE_NONE 15)

(define T_HEAVY 1)
(define T_TERRAIN 2)
(define T_OBSTACLE 4)
(define T_WEIGHTLESS 8)

(define R_DIE 0)
(define R_DROP 1)
(define R_MOVE 2)
(define R_PASS 3)

(define (base-draw r g b) (lambda (me z w) (draw me r g b z)))

(define (cadr x) (car (cdr x)))

(define (list . x) x)
