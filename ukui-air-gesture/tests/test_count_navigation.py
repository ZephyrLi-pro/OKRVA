import unittest,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'app/python'))
from reading_navigation import ReadingNavigationController
from reading_freshness import result_is_fresh
from test_reading_swipe import hand

def pose(x):return hand(.9775-x)

def run(xs,period=.25):
    controller=ReadingNavigationController();events=[]
    for i,x in enumerate(xs):
        direction=controller.update(None if x is None else pose(x),i*period)
        if direction:events.append(direction)
    return events,controller

class CountNavigationTests(unittest.TestCase):
    def test_three_left_three_right_count_only_the_outward_strokes(self):
        xs=[.5]*4
        for sign in (-1,-1,-1,1,1,1):
            xs += [.5+sign*d for d in (.08,.16,.22,.14,.06,0,0,0)]
        events,c=run(xs)
        self.assertEqual(events,['left']*3+['right']*3)
        self.assertEqual(c.accepted_counts,{'left':3,'right':3})
        self.assertEqual(c.event_id,6)

    def test_lost_fingers_during_return_do_not_count_right(self):
        events,c=run([.55]*4+[.475,.35,.2,None,.184,.515,.621,.617,.617])
        self.assertEqual(events,['left'])
        self.assertAlmostEqual(c.origin,.619)

    def test_mirrored_lost_fingers_during_return_do_not_count_left(self):
        events,_=run([.45]*4+[.525,.65,.8,None,.816,.485,.379,.383,.383])
        self.assertEqual(events,['right'])

    def test_return_overshoot_is_not_an_opposite_stroke(self):
        events,_=run([.5]*4+[.42,.34,.28,.4,.5,.61,.65,.65,.65])
        self.assertEqual(events,['left'])

    def test_real_opposite_stroke_after_return_confirmation_works(self):
        events,_=run([.5]*4+[.42,.34,.28,.4,.5,.61,.65,.65,.65,.73,.81,.85])
        self.assertEqual(events,['left','right'])

    def test_repeated_left_turnarounds_need_no_extra_side_hold(self):
        events,_=run([.5]*4+[.42,.34,.28,.4,.5,.42,.34,.28,.4,.5,.42,.34,.28])
        self.assertEqual(events,['left']*3)

    def test_repeated_right_turnarounds_need_no_extra_side_hold(self):
        events,_=run([.5]*4+[.58,.66,.72,.6,.5,.58,.66,.72,.6,.5,.58,.66,.72])
        self.assertEqual(events,['right']*3)

    def test_stationary_side_does_not_rearm(self):
        events,_=run([.5]*4+[.42,.34,.28]+[.28+(.01 if i%2 else -.01) for i in range(30)])
        self.assertEqual(events,['left'])

    def test_stable_offset_home_is_allowed(self):
        for home in (.35,.65):
            for sign,direction in ((-1,'left'),(1,'right')):
                with self.subTest(home=home,direction=direction):
                    events,_=run([home]*4+[home+sign*d for d in (.08,.16,.22,.14,.06,0,0)])
                    self.assertEqual(events,[direction])

    def test_noisy_warmup_and_center_jitter_do_not_count(self):
        events,_=run([.28,.2,.25,.32]+[.5]*4+[.5+(.02 if i%2 else -.02) for i in range(40)])
        self.assertEqual(events,[])

    def test_short_motion_is_not_a_command(self):
        events,_=run([.5]*4+[.53,.54,.53,.5,.5,.5])
        self.assertEqual(events,[])

    def test_long_loss_cannot_bridge_an_unfinished_trajectory(self):
        events,_=run([.5]*4+[.47]+[None]*16+[.29,.25,.3])
        self.assertEqual(events,[])

    def test_reacquisition_at_remembered_offset_home_can_rearm(self):
        events,_=run([.65]*4+[.62,None,.65,.65,.65,.73,.81,.85])
        self.assertEqual(events,['right'])

    def test_invalid_timestamp_does_not_duplicate_count(self):
        c=ReadingNavigationController()
        for i in range(4):c.update(pose(.5),i*.25)
        events=[c.update(pose(.42),1.),c.update(pose(.34),1.25)]
        for stamp in (1.25,1.2,float('nan')):
            self.assertIsNone(c.update(pose(.28),stamp))
        self.assertEqual([x for x in events if x],['left'])
        self.assertEqual(c.event_id,1)

    def test_neutral_jitter_does_not_poison_a_real_left_stroke(self):
        events,_=run([.5]*4+[.479,.512,.525,.407,.309])
        self.assertEqual(events,['left'])

    def test_neutral_jitter_does_not_poison_a_real_right_stroke(self):
        events,_=run([.5]*4+[.521,.488,.475,.593,.691])
        self.assertEqual(events,['right'])

    def test_threshold_adapts_to_palm_span_and_home_band_stays_inside_it(self):
        _,c=run([.5]*4)
        self.assertAlmostEqual(c.stroke_distance,.05)
        self.assertLess(c._home_radius(),c.stroke_distance)

    def test_small_natural_arc_is_a_stroke_but_large_vertical_motion_is_not(self):
        for dy,expected in ((-.052,['right']),(-.15,[])):
            c=ReadingNavigationController();events=[]
            for i,(x,y) in enumerate([(.5,.4)]*4+[(.54,.4),(.594,.4+dy),(.5,.4)]):
                direction=c.update(hand(.9775-x,y/.75+.06),i*.25)
                if direction:events.append(direction)
            self.assertEqual(events,expected)

    def test_short_pause_after_slow_reacquisition_allows_next_same_direction(self):
        c=ReadingNavigationController();events=[]
        samples=[(i*.25,x,.36) for i,x in enumerate([.5]*4+[.58,.66,.72])]
        samples += [(3.,.544,.374),(3.556,.531,.384),(3.791,.689,.351),(4.,.898,.363)]
        for now,x,y in samples:
            direction=c.update(hand(.9775-x,y/.75+.06),now)
            if direction:events.append(direction)
        self.assertEqual(events,['right','right'])

    def test_pause_halfway_back_does_not_rearm_an_opposite_command(self):
        events,_=run([.5]*4+[.58,.60,.54,.54,.54,.5,.46,.42,.5])
        self.assertEqual(events,['right'])

    def test_fast_observed_endpoint_counts_once_and_return_does_not(self):
        for sign,direction in ((-1,'left'),(1,'right')):
            xs=[.5]*4+[.5+sign*.32,.5+sign*.36,.5+sign*.12,.5,.5,.5]
            events,_=run(xs)
            self.assertEqual(events,[direction])

    def test_large_jump_without_confirmed_home_is_not_a_command(self):
        events,_=run([.5,.2,.5,.8])
        self.assertEqual(events,[])

    def test_fast_endpoint_with_a_large_palm_size_change_is_rejected(self):
        c=ReadingNavigationController()
        for i in range(4):c.update(pose(.5),i*.25)
        points=pose(.18)
        center=sum(points[i].x for i in (5,9,13,17))/4
        for point in points:point.x=center+(point.x-center)*.5
        self.assertIsNone(c.update(points,1.))
        self.assertEqual(c.event_id,0)

    def test_fast_endpoint_after_invalid_input_is_reacquisition_only(self):
        events,_=run([.5]*4+[None,.18,.18,.18])
        self.assertEqual(events,[])

    def test_fast_endpoint_with_large_vertical_displacement_is_rejected(self):
        c=ReadingNavigationController()
        for i in range(4):c.update(pose(.5),i*.25)
        self.assertIsNone(c.update(hand(.9775-.18,.7/.75+.06),1.))
        self.assertEqual(c.event_id,0)

    def test_result_age_accepts_normal_detection_but_rejects_delayed_or_invalid(self):
        for age in (0.,.22,.8,1.):self.assertTrue(result_is_fresh(age))
        for age in (-.01,1.001,2.,float('nan'),float('inf')):
            self.assertFalse(result_is_fresh(age))

    def test_large_stroke_allows_a_shifted_return_home(self):
        events,_=run([.4]*4+[.54,.74,.45,.50,.67,.56,.53,.53,.53,.60,.79])
        self.assertEqual(events,['right']*3)

    def test_mirrored_large_stroke_allows_a_shifted_return_home(self):
        events,_=run([.6]*4+[.46,.26,.55,.50,.33,.44,.47,.47,.47,.40,.21])
        self.assertEqual(events,['left']*3)

    def test_continuing_outward_after_sparse_count_cannot_rearm_itself(self):
        events,_=run([.5]*4+[.62,.83,.85,.84,.83])
        self.assertEqual(events,['right'])

    def test_return_overshoot_and_move_back_toward_home_is_not_a_new_stroke(self):
        events,_=run([.5]*4+[.58,.60,.5,.46,.42,.5])
        self.assertEqual(events,['right'])

    def test_observed_short_stroke_after_confirmed_home_is_counted(self):
        events,_=run([.5]*4+[.553,.7,.5,.5,.5])
        self.assertEqual(events,['right'])

    def test_horizontal_stroke_with_natural_arc_is_counted(self):
        c=ReadingNavigationController();events=[]
        for i,(x,y) in enumerate([(.5,.4)]*4+[(.612,.324)]):
            direction=c.update(hand(.9775-x,y/.75+.06),i*.25)
            if direction:events.append(direction)
        self.assertEqual(events,['right'])

    def test_predominantly_vertical_motion_is_not_counted(self):
        c=ReadingNavigationController();events=[]
        for i,(x,y) in enumerate([(.5,.4)]*4+[(.56,.32),(.6,.25)]):
            direction=c.update(hand(.9775-x,y/.75+.06),i*.25)
            if direction:events.append(direction)
        self.assertEqual(events,[])

    def test_partial_edge_hand_can_finish_only_a_recent_confirmed_outward_stroke(self):
        for home,edge,direction in ((.69,.964,'right'),(.31,.036,'left')):
            c=ReadingNavigationController()
            for i in range(4):c.update(pose(home),i*.25)
            self.assertEqual(c.update(pose(edge),1.),direction)
            self.assertTrue(c.edge_continuation)
            for i in range(1,5):self.assertIsNone(c.update(pose(edge),1.+i*.25))
            self.assertEqual(c.event_id,1)

    def test_new_partial_edge_hand_cannot_start_a_gesture(self):
        c=ReadingNavigationController()
        for i in range(8):self.assertIsNone(c.update(pose(.964),i*.25))
        self.assertEqual(c.event_id,0)

    def test_partial_edge_hand_after_input_loss_cannot_bridge_a_stroke(self):
        c=ReadingNavigationController()
        for i in range(4):c.update(pose(.69),i*.25)
        c.update(None,.9)
        self.assertIsNone(c.update(pose(.964),1.))
        self.assertEqual(c.event_id,0)

    def test_partial_edge_hand_requires_a_fresh_fully_visible_hand(self):
        c=ReadingNavigationController()
        for i in range(4):c.update(pose(.69),i*.25)
        self.assertIsNone(c.update(pose(.964),1.11))
        self.assertEqual(c.event_id,0)

    def test_deeply_clipped_or_closed_edge_hand_is_not_a_continuation(self):
        for points in (pose(1.05),hand(.9775-.964,closed=True)):
            c=ReadingNavigationController()
            for i in range(4):c.update(pose(.69),i*.25)
            self.assertIsNone(c.update(points,1.))
            self.assertEqual(c.event_id,0)

    def test_small_motion_waits_for_return_before_counting(self):
        c=ReadingNavigationController()
        for i in range(4):c.update(pose(.5),i*.25)
        self.assertIsNone(c.update(pose(.563),1.))
        self.assertEqual(c.accepted_counts['right'],0)
        self.assertEqual(c.phase,'confirming')
        self.assertEqual(c.update(pose(.51),1.25),'right')
        self.assertIsNone(c.update(pose(.5),1.5))
        self.assertEqual(c.accepted_counts,{'left':0,'right':1})

    def test_small_motion_continuing_into_a_clear_stroke_counts_once(self):
        events,_=run([.5]*4+[.563,.69,.72,.5,.5,.5])
        self.assertEqual(events,['right'])

    def test_continuous_full_hand_can_rotate_while_completing_a_clear_stroke(self):
        c=ReadingNavigationController()
        for i in range(4):c.update(pose(.5),i*.25)
        self.assertIsNone(c.update(pose(.563),1.))
        points=pose(.68);center=sum(points[i].x for i in (5,9,13,17))/4
        for point in points:point.x=center+(point.x-center)*.4
        self.assertEqual(c.update(points,1.25),'right')

    def test_small_motion_can_confirm_its_observed_return_after_brief_detection_loss(self):
        events,_=run([.5]*4+[.553,None,None,.5,.5])
        self.assertEqual(events,['right'])

    def test_small_motion_cannot_confirm_after_its_deadline(self):
        events,_=run([.5]*4+[.553]+[None]*13+[.5,.5,.5])
        self.assertEqual(events,[])

    def test_small_motion_return_survives_two_slow_detection_passes(self):
        c=ReadingNavigationController()
        for i in range(4):c.update(pose(.5),i*.25)
        self.assertIsNone(c.update(pose(.553),1.))
        c.update(None,1.7);c.update(None,2.4)
        self.assertEqual(c.update(pose(.5),3.35),'right')

    def test_confirmed_home_survives_a_slow_valid_sample_before_clear_outward_motion(self):
        for home,endpoint,direction in ((.68,.49,'left'),(.32,.51,'right')):
            c=ReadingNavigationController()
            for i in range(4):c.update(pose(home),i*.25)
            self.assertEqual(c.update(pose(endpoint),1.43),direction)
            self.assertEqual(c.event_id,1)

    def test_missing_input_does_not_use_the_slow_valid_sample_path(self):
        c=ReadingNavigationController()
        for i in range(4):c.update(pose(.68),i*.25)
        c.update(None,.9)
        self.assertIsNone(c.update(pose(.49),1.58))
        self.assertEqual(c.event_id,0)

    def test_long_interruption_does_not_use_the_slow_valid_sample_path(self):
        c=ReadingNavigationController()
        for i in range(4):c.update(pose(.68),i*.25)
        self.assertIsNone(c.update(pose(.49),2.))
        self.assertEqual(c.event_id,0)

    def test_recovery_rearms_at_a_nearby_shifted_home_without_an_action(self):
        c=ReadingNavigationController()
        for i in range(4):c.update(pose(.586),i*.25)
        c.update(None,1.)
        self.assertIsNone(c.update(pose(.63),1.25))
        self.assertIsNone(c.update(pose(.63),1.5))
        self.assertEqual(c.phase,'ready')
        self.assertAlmostEqual(c.origin,.63)
        self.assertEqual(c.event_id,0)

    def test_central_relocation_requires_a_longer_stationary_confirmation(self):
        c=ReadingNavigationController()
        for i in range(4):c.update(pose(.5),i*.25)
        c.update(None,1.)
        for stamp in (1.25,1.5,1.75):
            self.assertIsNone(c.update(pose(.65),stamp))
            self.assertEqual(c.phase,'recover_home')
        self.assertIsNone(c.update(pose(.65),2.))
        self.assertEqual(c.phase,'ready')
        self.assertEqual(c.event_id,0)
        self.assertEqual(c.update(pose(.5),2.25),'left')

    def test_waiting_far_outside_the_central_recovery_area_does_not_rearm(self):
        c=ReadingNavigationController()
        for i in range(4):c.update(pose(.5),i*.25)
        c.update(None,1.)
        for i in range(12):self.assertIsNone(c.update(pose(.85),1.25+i*.25))
        self.assertEqual(c.phase,'recover_home')
        self.assertEqual(c.event_id,0)

    def test_accepted_outer_endpoint_does_not_become_a_recovery_home(self):
        events,c=run([.5]*4+[.66,.72]+[.65]*8)
        self.assertEqual(events,['right'])
        self.assertEqual(c.phase,'returning')

    def test_unfinished_wobble_return_pair_rearms_before_next_real_stroke(self):
        c=ReadingNavigationController();events=[]
        xs=[.5859]*4+[.56218,.53285,.53379,.52959,.53250,.53517,.55875,.43266]
        for i,x in enumerate(xs):
            points=pose(x);center=sum(points[k].x for k in (5,9,13,17))/4
            for p in points:p.x=center+(p.x-center)*1.23
            direction=c.update(points,i*.25)
            if direction:events.append(direction)
            if i==len(xs)-2:self.assertEqual(c.phase,'ready')
        self.assertEqual(events,['left'])

    def test_sparse_clear_endpoint_accepts_palm_rotation_without_size_change(self):
        c=ReadingNavigationController()
        for i in range(4):c.update(pose(.5),i*.25)
        points=pose(.18);center=sum(points[k].x for k in (5,9,13,17))/4
        for p in points:
            dx=p.x-center;p.x=center+dx*.5;p.z=dx*(3**.5/2)
        self.assertEqual(c.update(points,1.),'left')

    def test_small_motion_closed_before_return_is_cancelled(self):
        c=ReadingNavigationController()
        for i in range(4):c.update(pose(.691483),i*.25)
        c.update(pose(.754086),1.)
        c.update(pose(.744665),1.25)
        self.assertIsNone(c.update(hand(.9775-.792240,closed=True),1.5))
        c.update(None,1.75)
        self.assertEqual(c.accepted_counts,{'left':0,'right':0})

    def test_five_real_strokes_followed_by_observed_small_withdrawal_stay_five(self):
        c=ReadingNavigationController();events=[]
        xs=[.691483]*4+([.85,.91,.80,.72,.691483,.691483]*5)
        for i,x in enumerate(xs):
            direction=c.update(pose(x),i*.25)
            if direction:events.append(direction)
        for offset,points in enumerate((pose(.754086),pose(.744665),
                hand(.9775-.792240,closed=True),None),len(xs)):
            direction=c.update(points,offset*.25)
            if direction:events.append(direction)
        self.assertEqual(events,['right']*5)

if __name__=='__main__':unittest.main()
