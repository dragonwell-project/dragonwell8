/*
 * Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "memory/allocation.inline.hpp"
#include "opto/addnode.hpp"
#include "opto/cfgnode.hpp"
#include "opto/connode.hpp"
#include "opto/loopnode.hpp"
#include "opto/phaseX.hpp"
#include "opto/runtime.hpp"
#include "opto/subnode.hpp"

// Portions of code courtesy of Clifford Click

// Optimization - Graph Style


extern int explicit_null_checks_elided;

//=============================================================================
//------------------------------Value------------------------------------------
// Return a tuple for whichever arm of the IF is reachable
const Type *IfNode::Value( PhaseTransform *phase ) const {
  if( !in(0) ) return Type::TOP;
  if( phase->type(in(0)) == Type::TOP )
    return Type::TOP;
  const Type *t = phase->type(in(1));
  if( t == Type::TOP )          // data is undefined
    return TypeTuple::IFNEITHER; // unreachable altogether
  if( t == TypeInt::ZERO )      // zero, or false
    return TypeTuple::IFFALSE;  // only false branch is reachable
  if( t == TypeInt::ONE )       // 1, or true
    return TypeTuple::IFTRUE;   // only true branch is reachable
  assert( t == TypeInt::BOOL, "expected boolean type" );

  return TypeTuple::IFBOTH;     // No progress
}

const RegMask &IfNode::out_RegMask() const {
  return RegMask::Empty;
}

//------------------------------split_if---------------------------------------
// Look for places where we merge constants, then test on the merged value.
// If the IF test will be constant folded on the path with the constant, we
// win by splitting the IF to before the merge point.
static Node* split_if(IfNode *iff, PhaseIterGVN *igvn) {
  // I could be a lot more general here, but I'm trying to squeeze this
  // in before the Christmas '98 break so I'm gonna be kinda restrictive
  // on the patterns I accept.  CNC

  // Look for a compare of a constant and a merged value
  Node *i1 = iff->in(1);
  if( !i1->is_Bool() ) return NULL;
  BoolNode *b = i1->as_Bool();
  Node *cmp = b->in(1);
  if( !cmp->is_Cmp() ) return NULL;
  i1 = cmp->in(1);
  if( i1 == NULL || !i1->is_Phi() ) return NULL;
  PhiNode *phi = i1->as_Phi();
  if( phi->is_copy() ) return NULL;
  Node *con2 = cmp->in(2);
  if( !con2->is_Con() ) return NULL;
  // See that the merge point contains some constants
  Node *con1=NULL;
  uint i4;
  for( i4 = 1; i4 < phi->req(); i4++ ) {
    con1 = phi->in(i4);
    if( !con1 ) return NULL;    // Do not optimize partially collapsed merges
    if( con1->is_Con() ) break; // Found a constant
    // Also allow null-vs-not-null checks
    const TypePtr *tp = igvn->type(con1)->isa_ptr();
    if( tp && tp->_ptr == TypePtr::NotNull )
      break;
  }
  if( i4 >= phi->req() ) return NULL; // Found no constants

  igvn->C->set_has_split_ifs(true); // Has chance for split-if

  // Make sure that the compare can be constant folded away
  Node *cmp2 = cmp->clone();
  cmp2->set_req(1,con1);
  cmp2->set_req(2,con2);
  const Type *t = cmp2->Value(igvn);
  // This compare is dead, so whack it!
  igvn->remove_dead_node(cmp2);
  if( !t->singleton() ) return NULL;

  // No intervening control, like a simple Call
  Node *r = iff->in(0);
  if( !r->is_Region() ) return NULL;
  if( phi->region() != r ) return NULL;
  // No other users of the cmp/bool
  if (b->outcnt() != 1 || cmp->outcnt() != 1) {
    //tty->print_cr("many users of cmp/bool");
    return NULL;
  }

  // Make sure we can determine where all the uses of merged values go
  for (DUIterator_Fast jmax, j = r->fast_outs(jmax); j < jmax; j++) {
    Node* u = r->fast_out(j);
    if( u == r ) continue;
    if( u == iff ) continue;
    if( u->outcnt() == 0 ) continue; // use is dead & ignorable
    if( !u->is_Phi() ) {
      /*
      if( u->is_Start() ) {
        tty->print_cr("Region has inlined start use");
      } else {
        tty->print_cr("Region has odd use");
        u->dump(2);
      }*/
      return NULL;
    }
    if( u != phi ) {
      // CNC - do not allow any other merged value
      //tty->print_cr("Merging another value");
      //u->dump(2);
      return NULL;
    }
    // Make sure we can account for all Phi uses
    for (DUIterator_Fast kmax, k = u->fast_outs(kmax); k < kmax; k++) {
      Node* v = u->fast_out(k); // User of the phi
      // CNC - Allow only really simple patterns.
      // In particular I disallow AddP of the Phi, a fairly common pattern
      if (v == cmp) continue;  // The compare is OK
      if (v->is_ConstraintCast()) {
        // If the cast is derived from data flow edges, it may not have a control edge.
        // If so, it should be safe to split. But follow-up code can not deal with
        // this (l. 359). So skip.
        if (v->in(0) == NULL) {
          return NULL;
        }
        if (v->in(0)->in(0) == iff) {
          continue;               // CastPP/II of the IfNode is OK
        }
      }
      // Disabled following code because I cannot tell if exactly one
      // path dominates without a real dominator check. CNC 9/9/1999
      //uint vop = v->Opcode();
      //if( vop == Op_Phi ) {     // Phi from another merge point might be OK
      //  Node *r = v->in(0);     // Get controlling point
      //  if( !r ) return NULL;   // Degraded to a copy
      //  // Find exactly one path in (either True or False doms, but not IFF)
      //  int cnt = 0;
      //  for( uint i = 1; i < r->req(); i++ )
      //    if( r->in(i) && r->in(i)->in(0) == iff )
      //      cnt++;
      //  if( cnt == 1 ) continue; // Exactly one of True or False guards Phi
      //}
      if( !v->is_Call() ) {
        /*
        if( v->Opcode() == Op_AddP ) {
          tty->print_cr("Phi has AddP use");
        } else if( v->Opcode() == Op_CastPP ) {
          tty->print_cr("Phi has CastPP use");
        } else if( v->Opcode() == Op_CastII ) {
          tty->print_cr("Phi has CastII use");
        } else {
          tty->print_cr("Phi has use I cant be bothered with");
        }
        */
      }
      return NULL;

      /* CNC - Cut out all the fancy acceptance tests
      // Can we clone this use when doing the transformation?
      // If all uses are from Phis at this merge or constants, then YES.
      if( !v->in(0) && v != cmp ) {
        tty->print_cr("Phi has free-floating use");
        v->dump(2);
        return NULL;
      }
      for( uint l = 1; l < v->req(); l++ ) {
        if( (!v->in(l)->is_Phi() || v->in(l)->in(0) != r) &&
            !v->in(l)->is_Con() ) {
          tty->print_cr("Phi has use");
          v->dump(2);
          return NULL;
        } // End of if Phi-use input is neither Phi nor Constant
      } // End of for all inputs to Phi-use
      */
    } // End of for all uses of Phi
  } // End of for all uses of Region

  // Only do this if the IF node is in a sane state
  if (iff->outcnt() != 2)
    return NULL;

  // Got a hit!  Do the Mondo Hack!
  //
  //ABC  a1c   def   ghi            B     1     e     h   A C   a c   d f   g i
  // R - Phi - Phi - Phi            Rc - Phi - Phi - Phi   Rx - Phi - Phi - Phi
  //     cmp - 2                         cmp - 2               cmp - 2
  //       bool                            bool_c                bool_x
  //       if                               if_c                  if_x
  //      T  F                              T  F                  T  F
  // ..s..    ..t ..                   ..s..    ..t..        ..s..    ..t..
  //
  // Split the paths coming into the merge point into 2 separate groups of
  // merges.  On the left will be all the paths feeding constants into the
  // Cmp's Phi.  On the right will be the remaining paths.  The Cmp's Phi
  // will fold up into a constant; this will let the Cmp fold up as well as
  // all the control flow.  Below the original IF we have 2 control
  // dependent regions, 's' and 't'.  Now we will merge the two paths
  // just prior to 's' and 't' from the two IFs.  At least 1 path (and quite
  // likely 2 or more) will promptly constant fold away.
  PhaseGVN *phase = igvn;

  // Make a region merging constants and a region merging the rest
  uint req_c = 0;
  Node* predicate_proj = NULL;
  for (uint ii = 1; ii < r->req(); ii++) {
    if (phi->in(ii) == con1) {
      req_c++;
    }
    Node* proj = PhaseIdealLoop::find_predicate(r->in(ii));
    if (proj != NULL) {
      assert(predicate_proj == NULL, "only one predicate entry expected");
      predicate_proj = proj;
    }
  }

  // If all the defs of the phi are the same constant, we already have the desired end state.
  // Skip the split that would create empty phi and region nodes.
  if((r->req() - req_c) == 1) {
    return NULL;
  }

  Node* predicate_c = NULL;
  Node* predicate_x = NULL;
  bool counted_loop = r->is_CountedLoop();

  Node *region_c = new (igvn->C) RegionNode(req_c + 1);
  Node *phi_c    = con1;
  uint  len      = r->req();
  Node *region_x = new (igvn->C) RegionNode(len - req_c);
  Node *phi_x    = PhiNode::make_blank(region_x, phi);
  for (uint i = 1, i_c = 1, i_x = 1; i < len; i++) {
    if (phi->in(i) == con1) {
      region_c->init_req( i_c++, r  ->in(i) );
      if (r->in(i) == predicate_proj)
        predicate_c = predicate_proj;
    } else {
      region_x->init_req( i_x,   r  ->in(i) );
      phi_x   ->init_req( i_x++, phi->in(i) );
      if (r->in(i) == predicate_proj)
        predicate_x = predicate_proj;
    }
  }
  if (predicate_c != NULL && (req_c > 1)) {
    assert(predicate_x == NULL, "only one predicate entry expected");
    predicate_c = NULL; // Do not clone predicate below merge point
  }
  if (predicate_x != NULL && ((len - req_c) > 2)) {
    assert(predicate_c == NULL, "only one predicate entry expected");
    predicate_x = NULL; // Do not clone predicate below merge point
  }

  // Register the new RegionNodes but do not transform them.  Cannot
  // transform until the entire Region/Phi conglomerate has been hacked
  // as a single huge transform.
  igvn->register_new_node_with_optimizer( region_c );
  igvn->register_new_node_with_optimizer( region_x );
  // Prevent the untimely death of phi_x.  Currently he has no uses.  He is
  // about to get one.  If this only use goes away, then phi_x will look dead.
  // However, he will be picking up some more uses down below.
  Node *hook = new (igvn->C) Node(4);
  hook->init_req(0, phi_x);
  hook->init_req(1, phi_c);
  phi_x = phase->transform( phi_x );

  // Make the compare
  Node *cmp_c = phase->makecon(t);
  Node *cmp_x = cmp->clone();
  cmp_x->set_req(1,phi_x);
  cmp_x->set_req(2,con2);
  cmp_x = phase->transform(cmp_x);
  // Make the bool
  Node *b_c = phase->transform(new (igvn->C) BoolNode(cmp_c,b->_test._test));
  Node *b_x = phase->transform(new (igvn->C) BoolNode(cmp_x,b->_test._test));
  // Make the IfNode
  IfNode *iff_c = new (igvn->C) IfNode(region_c,b_c,iff->_prob,iff->_fcnt);
  igvn->set_type_bottom(iff_c);
  igvn->_worklist.push(iff_c);
  hook->init_req(2, iff_c);

  IfNode *iff_x = new (igvn->C) IfNode(region_x,b_x,iff->_prob, iff->_fcnt);
  igvn->set_type_bottom(iff_x);
  igvn->_worklist.push(iff_x);
  hook->init_req(3, iff_x);

  // Make the true/false arms
  Node *iff_c_t = phase->transform(new (igvn->C) IfTrueNode (iff_c));
  Node *iff_c_f = phase->transform(new (igvn->C) IfFalseNode(iff_c));
  if (predicate_c != NULL) {
    assert(predicate_x == NULL, "only one predicate entry expected");
    // Clone loop predicates to each path
    iff_c_t = igvn->clone_loop_predicates(predicate_c, iff_c_t, !counted_loop);
    iff_c_f = igvn->clone_loop_predicates(predicate_c, iff_c_f, !counted_loop);
  }
  Node *iff_x_t = phase->transform(new (igvn->C) IfTrueNode (iff_x));
  Node *iff_x_f = phase->transform(new (igvn->C) IfFalseNode(iff_x));
  if (predicate_x != NULL) {
    assert(predicate_c == NULL, "only one predicate entry expected");
    // Clone loop predicates to each path
    iff_x_t = igvn->clone_loop_predicates(predicate_x, iff_x_t, !counted_loop);
    iff_x_f = igvn->clone_loop_predicates(predicate_x, iff_x_f, !counted_loop);
  }

  // Merge the TRUE paths
  Node *region_s = new (igvn->C) RegionNode(3);
  igvn->_worklist.push(region_s);
  region_s->init_req(1, iff_c_t);
  region_s->init_req(2, iff_x_t);
  igvn->register_new_node_with_optimizer( region_s );

  // Merge the FALSE paths
  Node *region_f = new (igvn->C) RegionNode(3);
  igvn->_worklist.push(region_f);
  region_f->init_req(1, iff_c_f);
  region_f->init_req(2, iff_x_f);
  igvn->register_new_node_with_optimizer( region_f );

  igvn->hash_delete(cmp);// Remove soon-to-be-dead node from hash table.
  cmp->set_req(1,NULL);  // Whack the inputs to cmp because it will be dead
  cmp->set_req(2,NULL);
  // Check for all uses of the Phi and give them a new home.
  // The 'cmp' got cloned, but CastPP/IIs need to be moved.
  Node *phi_s = NULL;     // do not construct unless needed
  Node *phi_f = NULL;     // do not construct unless needed
  for (DUIterator_Last i2min, i2 = phi->last_outs(i2min); i2 >= i2min; --i2) {
    Node* v = phi->last_out(i2);// User of the phi
    igvn->rehash_node_delayed(v); // Have to fixup other Phi users
    uint vop = v->Opcode();
    Node *proj = NULL;
    if( vop == Op_Phi ) {       // Remote merge point
      Node *r = v->in(0);
      for (uint i3 = 1; i3 < r->req(); i3++)
        if (r->in(i3) && r->in(i3)->in(0) == iff) {
          proj = r->in(i3);
          break;
        }
    } else if( v->is_ConstraintCast() ) {
      proj = v->in(0);          // Controlling projection
    } else {
      assert( 0, "do not know how to handle this guy" );
    }

    Node *proj_path_data, *proj_path_ctrl;
    if( proj->Opcode() == Op_IfTrue ) {
      if( phi_s == NULL ) {
        // Only construct phi_s if needed, otherwise provides
        // interfering use.
        phi_s = PhiNode::make_blank(region_s,phi);
        phi_s->init_req( 1, phi_c );
        phi_s->init_req( 2, phi_x );
        hook->add_req(phi_s);
        phi_s = phase->transform(phi_s);
      }
      proj_path_data = phi_s;
      proj_path_ctrl = region_s;
    } else {
      if( phi_f == NULL ) {
        // Only construct phi_f if needed, otherwise provides
        // interfering use.
        phi_f = PhiNode::make_blank(region_f,phi);
        phi_f->init_req( 1, phi_c );
        phi_f->init_req( 2, phi_x );
        hook->add_req(phi_f);
        phi_f = phase->transform(phi_f);
      }
      proj_path_data = phi_f;
      proj_path_ctrl = region_f;
    }

    // Fixup 'v' for for the split
    if( vop == Op_Phi ) {       // Remote merge point
      uint i;
      for( i = 1; i < v->req(); i++ )
        if( v->in(i) == phi )
          break;
      v->set_req(i, proj_path_data );
    } else if( v->is_ConstraintCast() ) {
      v->set_req(0, proj_path_ctrl );
      v->set_req(1, proj_path_data );
    } else
      ShouldNotReachHere();
  }

  // Now replace the original iff's True/False with region_s/region_t.
  // This makes the original iff go dead.
  for (DUIterator_Last i3min, i3 = iff->last_outs(i3min); i3 >= i3min; --i3) {
    Node* p = iff->last_out(i3);
    assert( p->Opcode() == Op_IfTrue || p->Opcode() == Op_IfFalse, "" );
    Node *u = (p->Opcode() == Op_IfTrue) ? region_s : region_f;
    // Replace p with u
    igvn->add_users_to_worklist(p);
    for (DUIterator_Last lmin, l = p->last_outs(lmin); l >= lmin;) {
      Node* x = p->last_out(l);
      igvn->hash_delete(x);
      uint uses_found = 0;
      for( uint j = 0; j < x->req(); j++ ) {
        if( x->in(j) == p ) {
          x->set_req(j, u);
          uses_found++;
        }
      }
      l -= uses_found;    // we deleted 1 or more copies of this edge
    }
    igvn->remove_dead_node(p);
  }

  // Force the original merge dead
  igvn->hash_delete(r);
  // First, remove region's dead users.
  for (DUIterator_Last lmin, l = r->last_outs(lmin); l >= lmin;) {
    Node* u = r->last_out(l);
    if( u == r ) {
      r->set_req(0, NULL);
    } else {
      assert(u->outcnt() == 0, "only dead users");
      igvn->remove_dead_node(u);
    }
    l -= 1;
  }
  igvn->remove_dead_node(r);

  // Now remove the bogus extra edges used to keep things alive
  igvn->remove_dead_node( hook );

  // Must return either the original node (now dead) or a new node
  // (Do not return a top here, since that would break the uniqueness of top.)
  return new (igvn->C) ConINode(TypeInt::ZERO);
}

//------------------------------is_range_check---------------------------------
// Return 0 if not a range check.  Return 1 if a range check and set index and
// offset.  Return 2 if we had to negate the test.  Index is NULL if the check
// is versus a constant.
int IfNode::is_range_check(Node* &range, Node* &index, jint &offset) {
  if (outcnt() != 2) {
    return 0;
  }
  Node* b = in(1);
  if (b == NULL || !b->is_Bool())  return 0;
  BoolNode* bn = b->as_Bool();
  Node* cmp = bn->in(1);
  if (cmp == NULL)  return 0;
  if (cmp->Opcode() != Op_CmpU)  return 0;

  Node* l = cmp->in(1);
  Node* r = cmp->in(2);
  int flip_test = 1;
  if (bn->_test._test == BoolTest::le) {
    l = cmp->in(2);
    r = cmp->in(1);
    flip_test = 2;
  } else if (bn->_test._test != BoolTest::lt) {
    return 0;
  }
  if (l->is_top())  return 0;   // Top input means dead test
  if (r->Opcode() != Op_LoadRange)  return 0;

  // We have recognized one of these forms:
  //  Flip 1:  If (Bool[<] CmpU(l, LoadRange)) ...
  //  Flip 2:  If (Bool[<=] CmpU(LoadRange, l)) ...

  // Make sure it's a real range check by requiring an uncommon trap
  // along the OOB path.  Otherwise, it's possible that the user wrote
  // something which optimized to look like a range check but behaves
  // in some other way.
  Node* iftrap = proj_out(flip_test == 2 ? true : false);
  bool found_trap = false;
  if (iftrap != NULL) {
    Node* u = iftrap->unique_ctrl_out();
    if (u != NULL) {
      // It could be a merge point (Region) for uncommon trap.
      if (u->is_Region()) {
        Node* c = u->unique_ctrl_out();
        if (c != NULL) {
          iftrap = u;
          u = c;
        }
      }
      if (u->in(0) == iftrap && u->is_CallStaticJava()) {
        int req = u->as_CallStaticJava()->uncommon_trap_request();
        if (Deoptimization::trap_request_reason(req) ==
            Deoptimization::Reason_range_check) {
          found_trap = true;
        }
      }
    }
  }
  if (!found_trap)  return 0;   // sorry, no cigar

  // Look for index+offset form
  Node* ind = l;
  jint  off = 0;
  if (l->is_top()) {
    return 0;
  } else if (l->Opcode() == Op_AddI) {
    if ((off = l->in(1)->find_int_con(0)) != 0) {
      ind = l->in(2);
    } else if ((off = l->in(2)->find_int_con(0)) != 0) {
      ind = l->in(1);
    }
  } else if ((off = l->find_int_con(-1)) >= 0) {
    // constant offset with no variable index
    ind = NULL;
  } else {
    // variable index with no constant offset (or dead negative index)
    off = 0;
  }

  // Return all the values:
  index  = ind;
  offset = off;
  range  = r;
  return flip_test;
}

//------------------------------adjust_check-----------------------------------
// Adjust (widen) a prior range check
static void adjust_check(Node* proj, Node* range, Node* index,
                         int flip, jint off_lo, PhaseIterGVN* igvn) {
  PhaseGVN *gvn = igvn;
  // Break apart the old check
  Node *iff = proj->in(0);
  Node *bol = iff->in(1);
  if( bol->is_top() ) return;   // In case a partially dead range check appears
  // bail (or bomb[ASSERT/DEBUG]) if NOT projection-->IfNode-->BoolNode
  DEBUG_ONLY( if( !bol->is_Bool() ) { proj->dump(3); fatal("Expect projection-->IfNode-->BoolNode"); } )
  if( !bol->is_Bool() ) return;

  Node *cmp = bol->in(1);
  // Compute a new check
  Node *new_add = gvn->intcon(off_lo);
  if( index ) {
    new_add = off_lo ? gvn->transform(new (gvn->C) AddINode( index, new_add )) : index;
  }
  Node *new_cmp = (flip == 1)
    ? new (gvn->C) CmpUNode( new_add, range )
    : new (gvn->C) CmpUNode( range, new_add );
  new_cmp = gvn->transform(new_cmp);
  // See if no need to adjust the existing check
  if( new_cmp == cmp ) return;
  // Else, adjust existing check
  Node *new_bol = gvn->transform( new (gvn->C) BoolNode( new_cmp, bol->as_Bool()->_test._test ) );
  igvn->rehash_node_delayed( iff );
  iff->set_req_X( 1, new_bol, igvn );
}

//------------------------------up_one_dom-------------------------------------
// Walk up the dominator tree one step.  Return NULL at root or true
// complex merges.  Skips through small diamonds.
Node* IfNode::up_one_dom(Node *curr, bool linear_only) {
  Node *dom = curr->in(0);
  if( !dom )                    // Found a Region degraded to a copy?
    return curr->nonnull_req(); // Skip thru it

  if( curr != dom )             // Normal walk up one step?
    return dom;

  // Use linear_only if we are still parsing, since we cannot
  // trust the regions to be fully filled in.
  if (linear_only)
    return NULL;

  if( dom->is_Root() )
    return NULL;

  // Else hit a Region.  Check for a loop header
  if( dom->is_Loop() )
    return dom->in(1);          // Skip up thru loops

  // Check for small diamonds
  Node *din1, *din2, *din3, *din4;
  if( dom->req() == 3 &&        // 2-path merge point
      (din1 = dom ->in(1)) &&   // Left  path exists
      (din2 = dom ->in(2)) &&   // Right path exists
      (din3 = din1->in(0)) &&   // Left  path up one
      (din4 = din2->in(0)) ) {  // Right path up one
    if( din3->is_Call() &&      // Handle a slow-path call on either arm
        (din3 = din3->in(0)) )
      din3 = din3->in(0);
    if( din4->is_Call() &&      // Handle a slow-path call on either arm
        (din4 = din4->in(0)) )
      din4 = din4->in(0);
    if (din3 != NULL && din3 == din4 && din3->is_If()) // Regions not degraded to a copy
      return din3;              // Skip around diamonds
  }

  // Give up the search at true merges
  return NULL;                  // Dead loop?  Or hit root?
}


//------------------------------filtered_int_type--------------------------------
// Return a possibly more restrictive type for val based on condition control flow for an if
const TypeInt* IfNode::filtered_int_type(PhaseGVN* gvn, Node *val, Node* if_proj) {
  assert(if_proj &&
         (if_proj->Opcode() == Op_IfTrue || if_proj->Opcode() == Op_IfFalse), "expecting an if projection");
  if (if_proj->in(0) && if_proj->in(0)->is_If()) {
    IfNode* iff = if_proj->in(0)->as_If();
    if (iff->in(1) && iff->in(1)->is_Bool()) {
      BoolNode* bol = iff->in(1)->as_Bool();
      if (bol->in(1) && bol->in(1)->is_Cmp()) {
        const CmpNode* cmp  = bol->in(1)->as_Cmp();
        if (cmp->in(1) == val) {
          const TypeInt* cmp2_t = gvn->type(cmp->in(2))->isa_int();
          if (cmp2_t != NULL) {
            jint lo = cmp2_t->_lo;
            jint hi = cmp2_t->_hi;
            BoolTest::mask msk = if_proj->Opcode() == Op_IfTrue ? bol->_test._test : bol->_test.negate();
            switch (msk) {
            case BoolTest::ne:
              // Can't refine type
              return NULL;
            case BoolTest::eq:
              return cmp2_t;
            case BoolTest::lt:
              lo = TypeInt::INT->_lo;
              if (hi - 1 < hi) {
                hi = hi - 1;
              }
              break;
            case BoolTest::le:
              lo = TypeInt::INT->_lo;
              break;
            case BoolTest::gt:
              if (lo + 1 > lo) {
                lo = lo + 1;
              }
              hi = TypeInt::INT->_hi;
              break;
            case BoolTest::ge:
              // lo unchanged
              hi = TypeInt::INT->_hi;
              break;
            }
            const TypeInt* rtn_t = TypeInt::make(lo, hi, cmp2_t->_widen);
            return rtn_t;
          }
        }
      }
    }
  }
  return NULL;
}

//------------------------------fold_compares----------------------------
// See if a pair of CmpIs can be converted into a CmpU.  In some cases
// the direction of this if is determined by the preceding if so it
// can be eliminate entirely.  Given an if testing (CmpI n c) check
// for an immediately control dependent if that is testing (CmpI n c2)
// and has one projection leading to this if and the other projection
// leading to a region that merges one of this ifs control
// projections.
//
//                   If
//                  / |
//                 /  |
//                /   |
//              If    |
//              /\    |
//             /  \   |
//            /    \  |
//           /    Region
//
Node* IfNode::fold_compares(PhaseGVN* phase) {
  if (Opcode() != Op_If) return NULL;

  Node* this_cmp = in(1)->in(1);
  if (this_cmp != NULL && this_cmp->Opcode() == Op_CmpI &&
      this_cmp->in(2)->is_Con() && this_cmp->in(2) != phase->C->top()) {
    Node* ctrl = in(0);
    BoolNode* this_bool = in(1)->as_Bool();
    Node* n = this_cmp->in(1);
    int hi = this_cmp->in(2)->get_int();
    if (ctrl != NULL && ctrl->is_Proj() && ctrl->outcnt() == 1 &&
        ctrl->in(0)->is_If() &&
        ctrl->in(0)->outcnt() == 2 &&
        ctrl->in(0)->in(1)->is_Bool() &&
        ctrl->in(0)->in(1)->in(1)->Opcode() == Op_CmpI &&
        ctrl->in(0)->in(1)->in(1)->in(2)->is_Con() &&
        ctrl->in(0)->in(1)->in(1)->in(2) != phase->C->top() &&
        ctrl->in(0)->in(1)->in(1)->in(1) == n) {
      IfNode* dom_iff = ctrl->in(0)->as_If();
      Node* otherproj = dom_iff->proj_out(!ctrl->as_Proj()->_con);
      if (otherproj->outcnt() == 1 && otherproj->unique_out()->is_Region() &&
          this_bool->_test._test != BoolTest::ne && this_bool->_test._test != BoolTest::eq) {
        // Identify which proj goes to the region and which continues on
        RegionNode* region = otherproj->unique_out()->as_Region();
        Node* success = NULL;
        Node* fail = NULL;
        for (int i = 0; i < 2; i++) {
          Node* proj = proj_out(i);
          if (success == NULL && proj->outcnt() == 1 && proj->unique_out() == region) {
            success = proj;
          } else if (fail == NULL) {
            fail = proj;
          } else {
            success = fail = NULL;
          }
        }
        if (success != NULL && fail != NULL && !region->has_phi()) {
          int lo = dom_iff->in(1)->in(1)->in(2)->get_int();
          BoolNode* dom_bool = dom_iff->in(1)->as_Bool();
          Node* dom_cmp =  dom_bool->in(1);
          const TypeInt* failtype  = filtered_int_type(phase, n, ctrl);
          if (failtype != NULL) {
            const TypeInt* type2 = filtered_int_type(phase, n, fail);
            if (type2 != NULL) {
              failtype = failtype->join(type2)->is_int();
            } else {
              failtype = NULL;
            }
          }

          if (failtype != NULL &&
              dom_bool->_test._test != BoolTest::ne && dom_bool->_test._test != BoolTest::eq) {
            int bound = failtype->_hi - failtype->_lo + 1;
            if (failtype->_hi != max_jint && failtype->_lo != min_jint && bound > 1) {
              // Merge the two compares into a single unsigned compare by building  (CmpU (n - lo) hi)
              BoolTest::mask cond = fail->as_Proj()->_con ? BoolTest::lt : BoolTest::ge;
              Node* adjusted = phase->transform(new (phase->C) SubINode(n, phase->intcon(failtype->_lo)));
              Node* newcmp = phase->transform(new (phase->C) CmpUNode(adjusted, phase->intcon(bound)));
              Node* newbool = phase->transform(new (phase->C) BoolNode(newcmp, cond));
              phase->is_IterGVN()->replace_input_of(dom_iff, 1, phase->intcon(ctrl->as_Proj()->_con));
              phase->hash_delete(this);
              set_req(1, newbool);
              return this;
            }
            if (failtype->_lo > failtype->_hi) {
              // previous if determines the result of this if so
              // replace Bool with constant
              phase->hash_delete(this);
              set_req(1, phase->intcon(success->as_Proj()->_con));
              return this;
            }
          }
        }
      }
    }
  }
  return NULL;
}

//------------------------------remove_useless_bool----------------------------
// Check for people making a useless boolean: things like
// if( (x < y ? true : false) ) { ... }
// Replace with if( x < y ) { ... }
static Node *remove_useless_bool(IfNode *iff, PhaseGVN *phase) {
  Node *i1 = iff->in(1);
  if( !i1->is_Bool() ) return NULL;
  BoolNode *bol = i1->as_Bool();

  Node *cmp = bol->in(1);
  if( cmp->Opcode() != Op_CmpI ) return NULL;

  // Must be comparing against a bool
  const Type *cmp2_t = phase->type( cmp->in(2) );
  if( cmp2_t != TypeInt::ZERO &&
      cmp2_t != TypeInt::ONE )
    return NULL;

  // Find a prior merge point merging the boolean
  i1 = cmp->in(1);
  if( !i1->is_Phi() ) return NULL;
  PhiNode *phi = i1->as_Phi();
  if( phase->type( phi ) != TypeInt::BOOL )
    return NULL;

  // Check for diamond pattern
  int true_path = phi->is_diamond_phi();
  if( true_path == 0 ) return NULL;

  // Make sure that iff and the control of the phi are different. This
  // should really only happen for dead control flow since it requires
  // an illegal cycle.
  if (phi->in(0)->in(1)->in(0) == iff) return NULL;

  // phi->region->if_proj->ifnode->bool->cmp
  BoolNode *bol2 = phi->in(0)->in(1)->in(0)->in(1)->as_Bool();

  // Now get the 'sense' of the test correct so we can plug in
  // either iff2->in(1) or its complement.
  int flip = 0;
  if( bol->_test._test == BoolTest::ne ) flip = 1-flip;
  else if( bol->_test._test != BoolTest::eq ) return NULL;
  if( cmp2_t == TypeInt::ZERO ) flip = 1-flip;

  const Type *phi1_t = phase->type( phi->in(1) );
  const Type *phi2_t = phase->type( phi->in(2) );
  // Check for Phi(0,1) and flip
  if( phi1_t == TypeInt::ZERO ) {
    if( phi2_t != TypeInt::ONE ) return NULL;
    flip = 1-flip;
  } else {
    // Check for Phi(1,0)
    if( phi1_t != TypeInt::ONE  ) return NULL;
    if( phi2_t != TypeInt::ZERO ) return NULL;
  }
  if( true_path == 2 ) {
    flip = 1-flip;
  }

  Node* new_bol = (flip ? phase->transform( bol2->negate(phase) ) : bol2);
  assert(new_bol != iff->in(1), "must make progress");
  iff->set_req(1, new_bol);
  // Intervening diamond probably goes dead
  phase->C->set_major_progress();
  return iff;
}

static IfNode* idealize_test(PhaseGVN* phase, IfNode* iff);

struct RangeCheck {
  Node* ctl;
  jint off;
};

//------------------------------Ideal------------------------------------------
// Return a node which is more "ideal" than the current node.  Strip out
// control copies
Node *IfNode::Ideal(PhaseGVN *phase, bool can_reshape) {
  if (remove_dead_region(phase, can_reshape))  return this;
  // No Def-Use info?
  if (!can_reshape)  return NULL;
  PhaseIterGVN *igvn = phase->is_IterGVN();

  // Don't bother trying to transform a dead if
  if (in(0)->is_top())  return NULL;
  // Don't bother trying to transform an if with a dead test
  if (in(1)->is_top())  return NULL;
  // Another variation of a dead test
  if (in(1)->is_Con())  return NULL;
  // Another variation of a dead if
  if (outcnt() < 2)  return NULL;

  // Canonicalize the test.
  Node* idt_if = idealize_test(phase, this);
  if (idt_if != NULL)  return idt_if;

  // Try to split the IF
  Node *s = split_if(this, igvn);
  if (s != NULL)  return s;

  // Check for people making a useless boolean: things like
  // if( (x < y ? true : false) ) { ... }
  // Replace with if( x < y ) { ... }
  Node *bol2 = remove_useless_bool(this, phase);
  if( bol2 ) return bol2;

  // Setup to scan up the CFG looking for a dominating test
  Node *dom = in(0);
  Node *prev_dom = this;

  // Check for range-check vs other kinds of tests
  Node *index1, *range1;
  jint offset1;
  int flip1 = is_range_check(range1, index1, offset1);
  if( flip1 ) {
    // Try to remove extra range checks.  All 'up_one_dom' gives up at merges
    // so all checks we inspect post-dominate the top-most check we find.
    // If we are going to fail the current check and we reach the top check
    // then we are guaranteed to fail, so just start interpreting there.
    // We 'expand' the top 3 range checks to include all post-dominating
    // checks.
    //
    // Example:
    // a[i+x] // (1) 1 < x < 6
    // a[i+3] // (2)
    // a[i+4] // (3)
    // a[i+6] // max = max of all constants
    // a[i+2]
    // a[i+1] // min = min of all constants
    //
    // If x < 3:
    //   (1) a[i+x]: Leave unchanged
    //   (2) a[i+3]: Replace with a[i+max] = a[i+6]: i+x < i+3 <= i+6  -> (2) is covered
    //   (3) a[i+4]: Replace with a[i+min] = a[i+1]: i+1 < i+4 <= i+6  -> (3) and all following checks are covered
    //   Remove all other a[i+c] checks
    //
    // If x >= 3:
    //   (1) a[i+x]: Leave unchanged
    //   (2) a[i+3]: Replace with a[i+min] = a[i+1]: i+1 < i+3 <= i+x  -> (2) is covered
    //   (3) a[i+4]: Replace with a[i+max] = a[i+6]: i+1 < i+4 <= i+6  -> (3) and all following checks are covered
    //   Remove all other a[i+c] checks
    //
    // We only need the top 2 range checks if x is the min or max of all constants.
    //
    // This, however, only works if the interval [i+min,i+max] is not larger than max_int (i.e. abs(max - min) < max_int):
    // The theoretical max size of an array is max_int with:
    // - Valid index space: [0,max_int-1]
    // - Invalid index space: [max_int,-1] // max_int, min_int, min_int - 1 ..., -1
    //
    // The size of the consecutive valid index space is smaller than the size of the consecutive invalid index space.
    // If we choose min and max in such a way that:
    // - abs(max - min) < max_int
    // - i+max and i+min are inside the valid index space
    // then all indices [i+min,i+max] must be in the valid index space. Otherwise, the invalid index space must be
    // smaller than the valid index space which is never the case for any array size.
    //
    // Choosing a smaller array size only makes the valid index space smaller and the invalid index space larger and
    // the argument above still holds.
    //
    // Note that the same optimization with the same maximal accepted interval size can also be found in C1.
    const jlong maximum_number_of_min_max_interval_indices = (jlong)max_jint;

    // The top 3 range checks seen
    const int NRC =3;
    RangeCheck prev_checks[NRC];
    int nb_checks = 0;

    // Low and high offsets seen so far
    jint off_lo = offset1;
    jint off_hi = offset1;

    bool found_immediate_dominator = false;

    // Scan for the top checks and collect range of offsets
    for (int dist = 0; dist < 999; dist++) { // Range-Check scan limit
      if (dom->Opcode() == Op_If &&  // Not same opcode?
          prev_dom->in(0) == dom) { // One path of test does dominate?
        if (dom == this) return NULL; // dead loop
        // See if this is a range check
        Node *index2, *range2;
        jint offset2;
        int flip2 = dom->as_If()->is_range_check(range2, index2, offset2);
        // See if this is a _matching_ range check, checking against
        // the same array bounds.
        if (flip2 == flip1 && range2 == range1 && index2 == index1 &&
            dom->outcnt() == 2) {
          if (nb_checks == 0 && dom->in(1) == in(1)) {
            // Found an immediately dominating test at the same offset.
            // This kind of back-to-back test can be eliminated locally,
            // and there is no need to search further for dominating tests.
            assert(offset2 == offset1, "Same test but different offsets");
            found_immediate_dominator = true;
            break;
          }

          // "x - y" -> must add one to the difference for number of elements in [x,y]
          const jlong diff = (jlong)MIN2(offset2, off_lo) - (jlong)MAX2(offset2, off_hi);
          if (ABS(diff) < maximum_number_of_min_max_interval_indices) {
            // Gather expanded bounds
            off_lo = MIN2(off_lo, offset2);
            off_hi = MAX2(off_hi, offset2);
            // Record top NRC range checks
            prev_checks[nb_checks % NRC].ctl = prev_dom;
            prev_checks[nb_checks % NRC].off = offset2;
            nb_checks++;
          }
        }
      }
      prev_dom = dom;
      dom = up_one_dom(dom);
      if (!dom) break;
    }

    if (!found_immediate_dominator) {
      // Attempt to widen the dominating range check to cover some later
      // ones.  Since range checks "fail" by uncommon-trapping to the
      // interpreter, widening a check can make us speculatively enter
      // the interpreter.  If we see range-check deopt's, do not widen!
      if (!phase->C->allow_range_check_smearing())  return NULL;

      // Didn't find prior covering check, so cannot remove anything.
      if (nb_checks == 0) {
        return NULL;
      }
      // Constant indices only need to check the upper bound.
      // Non-constant indices must check both low and high.
      int chk0 = (nb_checks - 1) % NRC;
      if (index1) {
        if (nb_checks == 1) {
          return NULL;
        } else {
          // If the top range check's constant is the min or max of
          // all constants we widen the next one to cover the whole
          // range of constants.
          RangeCheck rc0 = prev_checks[chk0];
          int chk1 = (nb_checks - 2) % NRC;
          RangeCheck rc1 = prev_checks[chk1];
          if (rc0.off == off_lo) {
            adjust_check(rc1.ctl, range1, index1, flip1, off_hi, igvn);
            prev_dom = rc1.ctl;
          } else if (rc0.off == off_hi) {
            adjust_check(rc1.ctl, range1, index1, flip1, off_lo, igvn);
            prev_dom = rc1.ctl;
          } else {
            // If the top test's constant is not the min or max of all
            // constants, we need 3 range checks. We must leave the
            // top test unchanged because widening it would allow the
            // accesses it protects to successfully read/write out of
            // bounds.
            if (nb_checks == 2) {
              return NULL;
            }
            int chk2 = (nb_checks - 3) % NRC;
            RangeCheck rc2 = prev_checks[chk2];
            // The top range check a+i covers interval: -a <= i < length-a
            // The second range check b+i covers interval: -b <= i < length-b
            if (rc1.off <= rc0.off) {
              // if b <= a, we change the second range check to:
              // -min_of_all_constants <= i < length-min_of_all_constants
              // Together top and second range checks now cover:
              // -min_of_all_constants <= i < length-a
              // which is more restrictive than -b <= i < length-b:
              // -b <= -min_of_all_constants <= i < length-a <= length-b
              // The third check is then changed to:
              // -max_of_all_constants <= i < length-max_of_all_constants
              // so 2nd and 3rd checks restrict allowed values of i to:
              // -min_of_all_constants <= i < length-max_of_all_constants
              adjust_check(rc1.ctl, range1, index1, flip1, off_lo, igvn);
              adjust_check(rc2.ctl, range1, index1, flip1, off_hi, igvn);
            } else {
              // if b > a, we change the second range check to:
              // -max_of_all_constants <= i < length-max_of_all_constants
              // Together top and second range checks now cover:
              // -a <= i < length-max_of_all_constants
              // which is more restrictive than -b <= i < length-b:
              // -b < -a <= i < length-max_of_all_constants <= length-b
              // The third check is then changed to:
              // -max_of_all_constants <= i < length-max_of_all_constants
              // so 2nd and 3rd checks restrict allowed values of i to:
              // -min_of_all_constants <= i < length-max_of_all_constants
              adjust_check(rc1.ctl, range1, index1, flip1, off_hi, igvn);
              adjust_check(rc2.ctl, range1, index1, flip1, off_lo, igvn);
            }
            prev_dom = rc2.ctl;
          }
        }
      } else {
        RangeCheck rc0 = prev_checks[chk0];
        // 'Widen' the offset of the 1st and only covering check
        adjust_check(rc0.ctl, range1, index1, flip1, off_hi, igvn);
        // Test is now covered by prior checks, dominate it out
        prev_dom = rc0.ctl;
      }
    }

  } else {                      // Scan for an equivalent test

    Node *cmp;
    int dist = 0;               // Cutoff limit for search
    int op = Opcode();
    if( op == Op_If &&
        (cmp=in(1)->in(1))->Opcode() == Op_CmpP ) {
      if( cmp->in(2) != NULL && // make sure cmp is not already dead
          cmp->in(2)->bottom_type() == TypePtr::NULL_PTR ) {
        dist = 64;              // Limit for null-pointer scans
      } else {
        dist = 4;               // Do not bother for random pointer tests
      }
    } else {
      dist = 4;                 // Limit for random junky scans
    }

    // Normal equivalent-test check.
    if( !dom ) return NULL;     // Dead loop?

    Node* result = fold_compares(phase);
    if (result != NULL) {
      return result;
    }

    // Search up the dominator tree for an If with an identical test
    while( dom->Opcode() != op    ||  // Not same opcode?
           dom->in(1)    != in(1) ||  // Not same input 1?
           (req() == 3 && dom->in(2) != in(2)) || // Not same input 2?
           prev_dom->in(0) != dom ) { // One path of test does not dominate?
      if( dist < 0 ) return NULL;

      dist--;
      prev_dom = dom;
      dom = up_one_dom( dom );
      if( !dom ) return NULL;
    }

    // Check that we did not follow a loop back to ourselves
    if( this == dom )
      return NULL;

    if( dist > 2 )              // Add to count of NULL checks elided
      explicit_null_checks_elided++;

  } // End of Else scan for an equivalent test

  // Hit!  Remove this IF
#ifndef PRODUCT
  if( TraceIterativeGVN ) {
    tty->print("   Removing IfNode: "); this->dump();
  }
  if( VerifyOpto && !phase->allow_progress() ) {
    // Found an equivalent dominating test,
    // we can not guarantee reaching a fix-point for these during iterativeGVN
    // since intervening nodes may not change.
    return NULL;
  }
#endif

  // Replace dominated IfNode
  dominated_by( prev_dom, igvn );

  // Must return either the original node (now dead) or a new node
  // (Do not return a top here, since that would break the uniqueness of top.)
  return new (phase->C) ConINode(TypeInt::ZERO);
}

//------------------------------dominated_by-----------------------------------
void IfNode::dominated_by( Node *prev_dom, PhaseIterGVN *igvn ) {
  igvn->hash_delete(this);      // Remove self to prevent spurious V-N
  Node *idom = in(0);
  // Need opcode to decide which way 'this' test goes
  int prev_op = prev_dom->Opcode();
  Node *top = igvn->C->top(); // Shortcut to top

  // Loop predicates may have depending checks which should not
  // be skipped. For example, range check predicate has two checks
  // for lower and upper bounds.
  ProjNode* unc_proj = proj_out(1 - prev_dom->as_Proj()->_con)->as_Proj();
  if ((unc_proj != NULL) && (unc_proj->is_uncommon_trap_proj(Deoptimization::Reason_predicate))) {
    prev_dom = idom;
  }

  // Now walk the current IfNode's projections.
  // Loop ends when 'this' has no more uses.
  for (DUIterator_Last imin, i = last_outs(imin); i >= imin; --i) {
    Node *ifp = last_out(i);     // Get IfTrue/IfFalse
    igvn->add_users_to_worklist(ifp);
    // Check which projection it is and set target.
    // Data-target is either the dominating projection of the same type
    // or TOP if the dominating projection is of opposite type.
    // Data-target will be used as the new control edge for the non-CFG
    // nodes like Casts and Loads.
    Node *data_target = (ifp->Opcode() == prev_op) ? prev_dom : top;
    // Control-target is just the If's immediate dominator or TOP.
    Node *ctrl_target = (ifp->Opcode() == prev_op) ?     idom : top;

    // For each child of an IfTrue/IfFalse projection, reroute.
    // Loop ends when projection has no more uses.
    for (DUIterator_Last jmin, j = ifp->last_outs(jmin); j >= jmin; --j) {
      Node* s = ifp->last_out(j);   // Get child of IfTrue/IfFalse
      if( !s->depends_only_on_test() ) {
        // Find the control input matching this def-use edge.
        // For Regions it may not be in slot 0.
        uint l;
        for( l = 0; s->in(l) != ifp; l++ ) { }
        igvn->replace_input_of(s, l, ctrl_target);
      } else {                      // Else, for control producers,
        igvn->replace_input_of(s, 0, data_target); // Move child to data-target
      }
    } // End for each child of a projection

    igvn->remove_dead_node(ifp);
  } // End for each IfTrue/IfFalse child of If

  // Kill the IfNode
  igvn->remove_dead_node(this);
}

//------------------------------Identity---------------------------------------
// If the test is constant & we match, then we are the input Control
Node *IfTrueNode::Identity( PhaseTransform *phase ) {
  // Can only optimize if cannot go the other way
  const TypeTuple *t = phase->type(in(0))->is_tuple();
  return ( t == TypeTuple::IFNEITHER || t == TypeTuple::IFTRUE )
    ? in(0)->in(0)              // IfNode control
    : this;                     // no progress
}

//------------------------------dump_spec--------------------------------------
#ifndef PRODUCT
void IfNode::dump_spec(outputStream *st) const {
  st->print("P=%f, C=%f",_prob,_fcnt);
}
#endif

//------------------------------idealize_test----------------------------------
// Try to canonicalize tests better.  Peek at the Cmp/Bool/If sequence and
// come up with a canonical sequence.  Bools getting 'eq', 'gt' and 'ge' forms
// converted to 'ne', 'le' and 'lt' forms.  IfTrue/IfFalse get swapped as
// needed.
static IfNode* idealize_test(PhaseGVN* phase, IfNode* iff) {
  assert(iff->in(0) != NULL, "If must be live");

  if (iff->outcnt() != 2)  return NULL; // Malformed projections.
  Node* old_if_f = iff->proj_out(false);
  Node* old_if_t = iff->proj_out(true);

  // CountedLoopEnds want the back-control test to be TRUE, irregardless of
  // whether they are testing a 'gt' or 'lt' condition.  The 'gt' condition
  // happens in count-down loops
  if (iff->is_CountedLoopEnd())  return NULL;
  if (!iff->in(1)->is_Bool())  return NULL; // Happens for partially optimized IF tests
  BoolNode *b = iff->in(1)->as_Bool();
  BoolTest bt = b->_test;
  // Test already in good order?
  if( bt.is_canonical() )
    return NULL;

  // Flip test to be canonical.  Requires flipping the IfFalse/IfTrue and
  // cloning the IfNode.
  Node* new_b = phase->transform( new (phase->C) BoolNode(b->in(1), bt.negate()) );
  if( !new_b->is_Bool() ) return NULL;
  b = new_b->as_Bool();

  PhaseIterGVN *igvn = phase->is_IterGVN();
  assert( igvn, "Test is not canonical in parser?" );

  // The IF node never really changes, but it needs to be cloned
  iff = new (phase->C) IfNode( iff->in(0), b, 1.0-iff->_prob, iff->_fcnt);

  Node *prior = igvn->hash_find_insert(iff);
  if( prior ) {
    igvn->remove_dead_node(iff);
    iff = (IfNode*)prior;
  } else {
    // Cannot call transform on it just yet
    igvn->set_type_bottom(iff);
  }
  igvn->_worklist.push(iff);

  // Now handle projections.  Cloning not required.
  Node* new_if_f = (Node*)(new (phase->C) IfFalseNode( iff ));
  Node* new_if_t = (Node*)(new (phase->C) IfTrueNode ( iff ));

  igvn->register_new_node_with_optimizer(new_if_f);
  igvn->register_new_node_with_optimizer(new_if_t);
  // Flip test, so flip trailing control
  igvn->replace_node(old_if_f, new_if_t);
  igvn->replace_node(old_if_t, new_if_f);

  // Progress
  return iff;
}

//------------------------------Identity---------------------------------------
// If the test is constant & we match, then we are the input Control
Node *IfFalseNode::Identity( PhaseTransform *phase ) {
  // Can only optimize if cannot go the other way
  const TypeTuple *t = phase->type(in(0))->is_tuple();
  return ( t == TypeTuple::IFNEITHER || t == TypeTuple::IFFALSE )
    ? in(0)->in(0)              // IfNode control
    : this;                     // no progress
}
