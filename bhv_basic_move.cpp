// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3, or (at your option)
 any later version.

 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this code; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "bhv_basic_move.h"

#include "strategy.h"

#include "bhv_basic_tackle.h"
#include "bhv_basic_block.h"

#include "basic_actions/basic_actions.h"
#include "basic_actions/body_go_to_point.h"
#include "basic_actions/body_intercept.h"
#include "basic_actions/neck_turn_to_ball_or_scan.h"
#include "basic_actions/neck_turn_to_low_conf_teammate.h"

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/intercept_table.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>

#include "neck_offensive_intercept_neck.h"

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_BasicMove::execute( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    //--------------------------------------------------------
    // tackle
    double do_tackle_prob = 0.8;
    if ( wm.ball().pos().x < 0.0 )
    {
        do_tackle_prob = 0.5;
    }

    if ( Bhv_BasicTackle( do_tackle_prob, 80.0 ).execute( agent ) )
    {
        // After successful tackle, check if ball is kickable
        const WorldModel & wm = agent->world();
        if ( wm.self().isKickable() )
        {
            // Ball is kickable after tackle, try to dribble or pass
            // For now, just turn to ball and prepare to control
            Body_TurnToBall().execute( agent );
            agent->setNeckAction( new Neck_TurnToBall() );
        }
        return true;
    }

    //--------------------------------------------------------
    // intercept
    int self_min = wm.interceptTable().selfStep();
    int mate_min = wm.interceptTable().teammateStep();
    int opp_min = wm.interceptTable().opponentStep();
    Vector2D getBallPos = wm.ball().inertiaPoint( self_min );
    
    if ( ! wm.existKickableTeammate()
         && self_min <= 3
         && getBallPos.x > -30.0 )
    {
        Body_Intercept().execute( agent );
        agent->setNeckAction( new Neck_OffensiveInterceptNeck() );

        return true;
    }

    //--------------------------------------------------------
    // non-DNN defensive block in our half
    if ( ! wm.self().goalie()
         && wm.ball().pos().x < -10.0
         && opp_min <= mate_min
         && self_min > opp_min + 1 )
    {
        if ( Bhv_BasicBlock().execute( agent ) )
        {
            return true;
        }
    }

    //--------------------------------------------------------
    // basic move
    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );
    const double dash_power = Strategy::get_normal_dash_power( wm );

    double dist_thr = wm.ball().distFromSelf() * 0.1;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_BasicMove target=(%.1f %.1f) dist_thr=%.2f",
                  target_point.x, target_point.y,
                  dist_thr );

    agent->debugClient().addMessage( "BasicMove%.0f", dash_power );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! Body_GoToPoint( target_point, dist_thr, dash_power
                           ).execute( agent ) )
    {
        Body_TurnToBall().execute( agent );
    }

    if ( wm.kickableOpponent()
         && wm.ball().distFromSelf() < 18.0 )
    {
        agent->setNeckAction( new Neck_TurnToBall() );
    }
    else
    {
        agent->setNeckAction( new Neck_TurnToBallOrScan( 0 ) );
    }

    return true;
}
