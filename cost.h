
#ifndef CASE_TYPE_COST
#define CASE_TYPE_COST(t, c) \
    case t:                  \
        return c
#endif

#ifdef COST_DELAY_BEGIN
#undef COST_DELAY_BEGIN

// Cost of inv types is set as that the total cost is the sum of
// this and cost of the base type.
CASE_TYPE_COST(AddInvTy, 0);
CASE_TYPE_COST(MulInvTy, 200);

CASE_TYPE_COST(AddTy, 100);
CASE_TYPE_COST(SubTy, 100);
CASE_TYPE_COST(MulTy, 300);
CASE_TYPE_COST(DivTy, 500);
CASE_TYPE_COST(RemTy, 500);

CASE_TYPE_COST(ShlTy, 20);
CASE_TYPE_COST(LshrTy, 20);
CASE_TYPE_COST(AshrTy, 20);
CASE_TYPE_COST(AndTy, 10);
CASE_TYPE_COST(OrTy, 10);
CASE_TYPE_COST(XorTy, 10);

CASE_TYPE_COST(EqTy, 10);
CASE_TYPE_COST(NeTy, 10);
CASE_TYPE_COST(GtTy, 100);
CASE_TYPE_COST(GeTy, 100);
CASE_TYPE_COST(LtTy, 100);
CASE_TYPE_COST(LeTy, 100);

CASE_TYPE_COST(SelectTy, 20);

#endif

#ifdef COST_AREA_BEGIN
#undef COST_AREA_BEGIN

CASE_TYPE_COST(ConstTy, 10);

CASE_TYPE_COST(AddInvTy, 0);
CASE_TYPE_COST(MulInvTy, 200);

CASE_TYPE_COST(AddTy, 100);
CASE_TYPE_COST(SubTy, 100);
CASE_TYPE_COST(MulTy, 300);
CASE_TYPE_COST(DivTy, 500);
CASE_TYPE_COST(RemTy, 500);

CASE_TYPE_COST(ShlTy, 20);
CASE_TYPE_COST(LshrTy, 20);
CASE_TYPE_COST(AshrTy, 20);
CASE_TYPE_COST(AndTy, 10);
CASE_TYPE_COST(OrTy, 10);
CASE_TYPE_COST(XorTy, 10);

CASE_TYPE_COST(EqTy, 10);
CASE_TYPE_COST(NeTy, 10);
CASE_TYPE_COST(GtTy, 100);
CASE_TYPE_COST(GeTy, 100);
CASE_TYPE_COST(LtTy, 100);
CASE_TYPE_COST(LeTy, 100);

CASE_TYPE_COST(SelectTy, 20);

#endif
