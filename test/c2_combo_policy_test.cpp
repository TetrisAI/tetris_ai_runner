#include "../cultris/c2_ai_impl.cpp"
#include <cmath>
#include <cstdio>

int failures=0;
void expect(bool ok,const char* message){if(!ok){++failures;std::printf("FAIL: %s\n",message);}}
bool close(double a,double b){return std::abs(a-b)<1e-9;}

int main(){
    CultrisEngine engine;engine.prepare(10,28);install_c2_config_once(engine);
    C2StepInput input{};input.combo_count=5;
    input.reserved[C2_INPUT_PLANNING_BPM]=240;
    input.reserved[C2_INPUT_COMBO_REMAINING_MS]=500;
    update_c2_config_per_frame(engine,input);
    expect(engine.status()->combo==5,"host root preserves the real combo even when continuation is poor");
    expect(close(engine.status()->combo_limit,.5),"host root preserves remaining time");
    expect(input.combo_count==5,"host snapshot remains immutable");
    input.incoming_garbage=1;
    expect(update_c2_config_per_frame(engine,input),"danger start invalidates candidate scores");
    input.incoming_garbage=0;
    expect(update_c2_config_per_frame(engine,input),"danger end invalidates candidate scores without rewriting root");
    expect(engine.status()->combo==5,"danger transition keeps real root combo");
    expect(!update_c2_config_per_frame(engine,input),"unchanged policy does not invalidate every frame");

    ai_zzz::C2::Config cfg{};cfg.mode=0;cfg.soft_drop=1;cfg.p_rate=1;cfg.planning_bpm=240;
    ai_zzz::C2 ai;ai.init<rule_c2::TetrisRule::rule_spec>(&cfg);
    ai_zzz::C2::Status parent{};parent.combo=4;parent.combo_limit=1.2;parent.attack=123;parent.map=456;
    ai_zzz::C2::Result poor{};poor.fill=.3;poor.hole_count=2;
    poor.cells=0;poor.map=-100;
    m_tetris2::TetrisMap<10,28> src_map{};
    const auto saved=parent;
    const auto stop=ai.get(poor,ai_zzz::C2::Landing{},1,src_map,parent,' ',0);
    auto rich=poor;rich.cells=180;
    const auto keep=ai.get(rich,ai_zzz::C2::Landing{},1,src_map,parent,' ',0);
    expect(stop.combo==0,"poor candidate clears only returned continuation combo");
    expect(keep.combo==5,"rich candidate preserves returned continuation combo");
    expect(close(stop.value,keep.value)&&close(stop.attack,keep.attack),
        "current placement is scored using input combo, not DP-pruned output combo");
    expect(close(stop.combo_limit,keep.combo_limit),"DP pruning does not fabricate deadline");
    expect(parent.combo==saved.combo&&close(parent.combo_limit,saved.combo_limit)
        &&close(parent.attack,saved.attack)&&close(parent.map,saved.map),"evaluator parent is immutable");
    auto next_piece=poor;next_piece.hole_count=10;
    const auto stopped_next=ai.get(next_piece,ai_zzz::C2::Landing{},1,src_map,stop,' ',0);
    const auto continued_next=ai.get(next_piece,ai_zzz::C2::Landing{},1,src_map,keep,' ',0);
    expect(!close(stopped_next.attack-stop.attack,continued_next.attack-keep.attack),
        "returned continuation state changes next placement scoring");
    cfg.danger=1;
    const auto danger=ai.get(poor,ai_zzz::C2::Landing{},1,src_map,parent,' ',0);
    expect(danger.combo==5,"incoming danger bypasses continuation pruning");
    cfg.mode=1;
    const auto downstack=ai.get(poor,ai_zzz::C2::Landing{},1,src_map,parent,' ',0);
    expect(downstack.combo==4,"Downstack mode is untouched by Attack continuation policy");

    // Evaluate a legal O on an empty board, with exact post-placement rows.
    using Details=m_tetris2::TetrisNodeDetails<rule_c2::TetrisRule::rule_spec,'O',0>;
    m_tetris2::TetrisNode<Details,std::monostate> node{};
    node.x=0;node.y=m_tetris2::TetrisNodeBase<Details>::kOriginY;
    CultrisMap empty{},after{};
    after.set_row(0,static_cast<CultrisMap::row_t>(0x3ff ^ 3));
    after.set_row(1,static_cast<CultrisMap::row_t>(0x3ff ^ 3));
    (void)node;(void)empty;
    const auto features=ai.eval(after);
    expect(features.cells==4,"evaluator carries exact post-placement population");
    // Real search tree: danger end must not reuse old candidate scores.
    CultrisMap board{};
    m_tetris2::TetrisBlockStatus current('I',3,20,0);
    input.incoming_garbage=1;
    if(update_c2_config_per_frame(engine,input))engine.update();
    auto first=engine.run(board,current,"",0,1);
    expect(CultrisEngine::TreeNode::lp_is_valid(first.target),"search produces candidate with danger");
    const auto version=engine.search_version();
    input.incoming_garbage=0;
    if(update_c2_config_per_frame(engine,input))engine.update();
    auto updated=engine.run(board,current,"",0,1);
    expect(engine.search_version()!=version,"danger end updates evaluation version");
    expect(engine.status()->combo==5,"real search root remains combo 5");
    expect(CultrisEngine::TreeNode::lp_is_valid(updated.target)&&updated.status.combo==0,
        "real tree returns DP-pruned candidate after danger end");
    engine.update();
    auto fresh=engine.run(board,current,"",0,1);
    expect(updated.status.combo==fresh.status.combo&&close(updated.status.value,fresh.status.value),
        "policy change result matches independently refreshed tree");
    std::printf("%s c2_combo_policy_test (%d failures)\n",failures?"FAIL":"PASS",failures);
    return failures!=0;
}
