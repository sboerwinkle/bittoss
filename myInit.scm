(define MOVE_ME 1)
(define MOVE_HIM 2)
(define MOVE_BOTH 7)
(define MOVE_NONE 15)

(define T_HEAVY 1)
(define T_TERRAIN 2)
(define T_OBSTACLE 4)
(define T_WEIGHTLESS 8)
; (define T_GROW 16)
(define T_FLAG 16)
(define TEAM_BIT 32)
(define TEAM_MASK (* TEAM_BIT 7))

(define R_DIE 0)
(define R_DROP 1)
(define R_MOVE 2)
(define R_PASS 3)

(define (cadr x) (car (cdr x)))

(define (list . x) x)

(define (foldr f x lst)
     (if (null? lst)
          x
          (foldr f (f x (car lst)) (cdr lst))))

(define (unzip1 lists)
	(if (null? lists)
		(cons '() '())
		(let
			(
				(list1 (car lists))
				(unzipped (unzip1 (cdr lists)))
			)
			(cons (cons (car list1) (car unzipped)) (cons (cdr list1) (cdr unzipped)))
		)
	)
)

(define (map proc . lists)
  (if (null? lists)
      (apply proc)
      (if (null? (car lists))
        '()
        (let* ((unz (unzip1 lists))
               (cars (car unz))
               (cdrs (cdr unz)))
          (cons (apply proc cars) (apply map (cons proc cdrs)))))))

(define (abs x) (if (>= x 0) x (- 0 x)))

; Not as fundamental as some of these others, perhaps, but helpful for the kind of stuff we're doing
(define (bound x b) (if (> x b) b (if (< x (- b)) (- b) x)))
