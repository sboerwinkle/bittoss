;(define (ground-who-moves a b axis dir) MOVE_HIM) Moot - he has no collideMask
(define ground-draw (base-draw 0.5 0.25 0.0))
(define (mk-ground pos)
  (mk-ground-inner pos '(512 512 512))
)

;int32_t r[3] = {16 * PTS_PER_PX, 16 * PTS_PER_PX, 16 * PTS_PER_PX};
;return createHelper(sc, args, NULL, r, T_TERRAIN | T_HEAVY | T_WEIGHTLESS, 0);
(define (mk-ground-inner pos r draw-func)
	(set-draw
		; Owner is '()
		; Collide mask 0
		; num sliders 0
		(create '() r (+ T_TERRAIN T_HEAVY T_WEIGHTLESS) 0 pos 0)
		draw-func
	)
)
