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
			0
		)
	draw-func) platform-whomoves)
)
