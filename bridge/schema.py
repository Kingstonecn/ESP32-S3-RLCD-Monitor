from __future__ import annotations

from datetime import datetime
from typing import Optional

from pydantic import BaseModel, Field


class ActiveBlock(BaseModel):
    started_at: datetime
    ends_at: datetime
    tokens_used: int
    cost_usd: float
    tokens_limit: Optional[int] = None
    percent_used: Optional[float] = None
    minutes_remaining: int
    projection_tokens: Optional[int] = None
    projection_cost_usd: Optional[float] = None


class Bucket(BaseModel):
    tokens_used: int
    cost_usd: float
    percent_used: Optional[float] = None
    tokens_limit: Optional[int] = None


class ModelBreakdown(BaseModel):
    model: str
    tokens: int
    cost_usd: float


class ClaudeUsage(BaseModel):
    active_block: Optional[ActiveBlock] = None
    weekly: Bucket
    today: Bucket
    month: Bucket
    lifetime: Bucket
    by_model: list[ModelBreakdown] = Field(default_factory=list)


class OtherAgentUsage(BaseModel):
    agent: str
    today: Bucket
    month: Bucket
    lifetime: Bucket


class UsageReport(BaseModel):
    updated_at: datetime
    source: str = "ccusage"
    claude: ClaudeUsage
    other: list[OtherAgentUsage] = Field(default_factory=list)
