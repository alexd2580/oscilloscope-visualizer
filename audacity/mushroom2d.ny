(defun rect (hz pre duration post)
    (let
        ((hz_factor (/ 1.0 hz)))
        (control-srate-abs *sound-srate* (seq
            (const 0 (* hz_factor pre))
            (const 1 (* hz_factor duration))
            (const 0 (* hz_factor post))
        ))
    )
)

(defun cosine (hz) (hzosc hz *table* 90.0))

(defun stem_factor () (scale 0.1 (rect 50.0 0.35 0.65 0.0)))
(defun head_factor () (mult (hzosc 50) (rect 50.0 0.0 0.35 0.65)))
(defun mush_x () (mult (hzosc 2500) (sum (stem_factor) (head_factor))))
(defun mush_y () (mult (osc-saw 50) (rect 50.0 0.0 1.0 0.0)))
(defun mush_z () (mult (cosine 2500) (sum (stem_factor) (head_factor))))


;(vector
;(seqrep (var 2) (hzosc 1))
;(force-srate 44100 (stretch-abs 2 (sound (hzosc 1))))
;(stretch-abs 2.0 (sound (hzosc 1)))
; (force-srate 22050 (sound (hzosc 1)))
;)

; (force-srate HZ (sound SOUND))
; Stretches the selected area.
; 22050 (half)
; 44100 (default) Leaves the sound as is.
;   0.25s -> 0.0625
;   0.5s -> 0.25s
;   1s -> 1s
;   2s -> 4s
;   4s -> 16s
; 88200 (double)
;   0.25s -> 0.125
;   0.5s -> 0.5s
;   1s -> 2s
;   2s -> 8s
;   4s -> 32s

(defun yaw (x y z a)
    (vector
        (sum (mult x (cos a)) (mult y (sin (- 0 a))))
        (sum (mult x (sin a)) (mult y (cos a)))
        z
    )
)

(defun yaw-continuous (vec3 a)
    (let
        ((x (aref vec3 0)) (y (aref vec3 1)) (z (aref vec3 2)))
        (vector
            (sum (mult x (cosine a)) (mult (- 0.0 1.0) (mult y (hzosc a))))
            (sum (mult x (hzosc a)) (mult y (cosine a)))
            z
        )
    )
)

(defun pitch-continuous (vec3 a)
    (let
        ((x (aref vec3 0)) (y (aref vec3 1)) (z (aref vec3 2)))
        (vector
            (sum (mult z (hzosc a)) (mult x (cosine a)))
            y
            (sum (mult z (hzosc a)) (mult (- 0.0 1.0) (mult x (cosine a))))
        )
    )
)

(defun project (vec3 n w h)
    ; Ignore Z for now.
    ; https://jsantell.com/3d-projection/
    ; (vector (mult (/ (* 2.0 n) w) x) (mult (/ (* 2.0 n) h) y))
    (vector (aref vec3 0) (aref vec3 1))

)

(defun repvec (vec) (seqrep (var 1000) vec))
(defun repvec3 (vec3) (vector (repvec (aref vec3 0)) (repvec (aref vec3 1)) (repvec (aref vec3 2))))

(let*
    (
        (mush (vector (mush_x) (mush_y) (mush_z)))
        (mush_rep (repvec3 mush))
        (yawed (yaw-continuous mush_rep 1.0)) ; 1hz rotation
        (pitched (pitch-continuous yawed 1.0)) ; 1hz rotation
        (projected (project pitched 1.0 1.0 1.0))
    )
    projected
)
