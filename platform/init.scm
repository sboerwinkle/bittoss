;(define (ground-who-moves a b axis dir) MOVE_HIM) Moot - he has no collideMask
(define (platform-who-moves a b axis dir) (if (typ? b T_TERRAIN) MOVE_ME MOVE_HIM))

(define platform-size '(3200 3200 512))

(define (mk-platform src pos draw-func)
	(set-who-moves (set-draw
		; num sliders 0
		(create
			src
			platform-size
			(+ T_TERRAIN T_HEAVY T_WEIGHTLESS)
			T_TERRAIN
			pos
			0
		)
	draw-func) platform-who-moves)
)
