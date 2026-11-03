import unittest
from reading_navigation import ReadingNavigationController
from test_reading_navigation import pose

def detect(xs):
    controller=ReadingNavigationController();events=[]
    for i,x in enumerate(xs):
        event=controller.update(pose(x),i*.25)
        if event:events.append(event)
    return events

class NeutralPositionTests(unittest.TestCase):
    def test_home_is_acquired_at_fast_and_slow_inference_rates(self):
        for period in (.05,.1,.25,.4):
            for home in (.4,.42,.58,.6):
                with self.subTest(period=period,home=home):
                    controller=ReadingNavigationController()
                    for i in range(20):
                        self.assertIsNone(controller.update(pose(home),i*period))
                    self.assertAlmostEqual(controller.origin,home)

    def test_repeated_relative_excursions_count_only_outward_motion(self):
        for home in (.42,.58):
            xs=[home]*3
            for _ in range(3):
                for sign in (-1,1):
                    xs += [home+sign*d for d in (.04,.08,.12,.08,.04,0)]
            self.assertEqual(detect(xs),['left','right']*3)

    def test_same_stroke_distance_works_both_ways_from_offset_home(self):
        for home in (.42,.58):
            for sign,direction in ((-1,'left'),(1,'right')):
                with self.subTest(home=home,direction=direction):
                    xs=[home]*3+[home+sign*d for d in (.04,.08,.12,.08,.04,0)]+[home]*3
                    self.assertEqual(detect(xs),[direction])

    def test_returning_via_old_center_edge_does_not_count_opposite(self):
        self.assertEqual(detect([.5]*3+[.42,.34,.28,.4,.4,.4,.5,.5,.5]),['left'])
        self.assertEqual(detect([.5]*3+[.58,.66,.72,.6,.6,.6,.5,.5,.5]),['right'])

    def test_quiet_new_home_rearms_without_a_phantom_stroke(self):
        xs=[.42]*3+[.46,.50,.50,.50,.54,.58,.62,.58,.54,.50]
        self.assertEqual(detect(xs),['right'])

    def test_small_jitter_from_offset_home_never_flips(self):
        for home in (.42,.58):
            self.assertEqual(detect([home]*3+[home+(.02 if i%2 else -.02) for i in range(80)]),[])

if __name__=='__main__':unittest.main()
