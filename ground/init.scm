;(define (ground-who-moves a b axis dir) MOVE_HIM) Moot - he has no collideMask
(define ground-draw (base-draw 0.5 0.25 0.0))
(define (mk-ground pos)
  (set-draw
    (ground-create-tmp pos 0)
    ground-draw
  )
)
