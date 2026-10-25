import unittest
import numpy as np
from scripts.eth_trees.tuning_graph import infer_forest, quality, errors


class ForestTests(unittest.TestCase):
    def test_path_order_inverse_and_rejection_of_weak_shortcut(self):
        angle=.4
        ab=np.eye(4);ab[:2,:2]=[[np.cos(angle),-np.sin(angle)],[np.sin(angle),np.cos(angle)]];ab[:3,3]=[2,-1,.4]
        bc=np.eye(4);bc[:3,3]=[-.5,3,.2]
        bad=np.eye(4);bad[:3,3]=[99,99,99]
        t,paths,chosen=infer_forest(['a','b','c','isolated'],[
            dict(a='a',b='b',T=ab,score=10),dict(a='b',b='c',T=bc,score=9),
            dict(a='a',b='c',T=bad,score=1)])
        np.testing.assert_allclose(t['a','c'],bc@ab)
        np.testing.assert_allclose(t['c','a']@t['a','c'],np.eye(4),atol=1e-12)
        self.assertEqual(paths['a','c'],['a','b','c'])
        self.assertEqual(len(chosen),2)
        self.assertNotIn(('a','isolated'),t)

    def test_quality_uses_3d_geometry_and_rejects_tilted_bad_transform(self):
        p=np.array([[i, i*i*.2, .3*(i%2)] for i in range(8)],dtype=float)
        good=np.eye(4);good[:3,3]=[3,-2,.5];q=p+good[:3,3]
        self.assertGreater(quality(good,p,q,6)['score'],0)
        wrong=good.copy();wrong[2,3]+=1
        self.assertEqual(quality(wrong,p,q,6)['score'],0)
        self.assertEqual(quality(good,p,q,3)['score'],0)
        self.assertEqual(errors(good,good),(0.,0.))


if __name__=='__main__':unittest.main()
